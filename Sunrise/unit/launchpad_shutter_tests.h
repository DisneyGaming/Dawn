#pragma once
#include "../src/state/activity/Newlight/launchpad/shutter_native.h"

namespace launchpad_shutter_tests {
namespace sh=lp::shutter;
namespace gn=sh::gn;
struct Fixture {
    static constexpr std::uintptr_t source=0x1000,physical=0x2000,logical=0x3000,row=0x4000,logicalRow=0x5000,image=0x100000;
    std::array<std::byte,0x6000> memory{};
    gn::Weak entity{71,5},device{72,6},logicalEntity{73,7},logicalDevice{74,8};
    unsigned exchanges{},enables{};std::uint32_t authority{0x80000000U};bool race{};
    template<class T> void put(std::uintptr_t a,T v) {std::memcpy(memory.data()+a,&v,sizeof v);}
    template<class T> bool value(std::uintptr_t a,T& v) {
        if(a>memory.size() || sizeof v>memory.size()-a) {return false;}
        std::memcpy(&v,memory.data()+a,sizeof v);return true;
    }
    bool copy(std::uintptr_t a,std::span<std::byte> out) {
        if(a>memory.size() || out.size()>memory.size()-a) {return false;}
        std::memcpy(out.data(),memory.data()+a,out.size());return true;
    }
    bool weak(gn::Weak v) {return v==entity || v==device || v==logicalEntity || v==logicalDevice;}
    bool make_weak(std::uint32_t h,gn::Weak& out) {
        for(const auto v:{entity,device,logicalEntity,logicalDevice}) {if(v.handle==h) {out=v;return true;}}return false;
    }
    bool entity_row(gn::Weak v,std::uintptr_t& out) {
        if(v==entity) {out=row;return true;}if(v==logicalEntity) {out=logicalRow;return true;}return false;
    }
    bool resolve(std::uint32_t h,std::uintptr_t& out) {
        if(h==device.handle) {out=physical;return true;}if(h==logicalDevice.handle) {out=logical;return true;}return false;
    }
    std::uint64_t compare_exchange(std::uintptr_t a,std::uint64_t expected,std::uint64_t desired) {
        CHECK(a==source+0x1F0);++exchanges;std::uint64_t current{};CHECK(value(a,current));
        if(race) {return ~expected;}if(current==expected) {put(a,desired);}return current;
    }
    void enable(std::uintptr_t a,std::uint32_t mask) {
        CHECK(a==image+0x26BE0E0);CHECK(mask==(1U<<entity.handle));++enables;authority|=mask;
    }
    sh::Physical target() {return {entity,device,physical};}
    Fixture() {
        put(source,gn::Ref{sh::kGate,0x80804F46U,0x278});put(source+0x1F0,logicalDevice);
        put(physical,gn::Ref{sh::kDevice,0x80803910U,0xA78});put(physical+0x24,device.handle);put(physical+0x2C,entity.handle);
        put(logical,gn::Ref{sh::kLogicalDevice,0x80803910U,0xA78});put(logical+0x24,logicalDevice.handle);put(logical+0x2C,logicalEntity.handle);
        put(row+0xC,entity.handle);put(logicalRow+0xC,logicalEntity.handle);
        const std::array<std::uint32_t,4> origin{1,2,3,4};put(row+0xD0,origin);put(logicalRow+0xD0,origin);
    }
    sh::Bound bind() {return sh::bind(*this,*this,image,source,target());}
};
inline void verify() {
    Fixture f;CHECK(f.bind()==sh::Bound::changed);CHECK(f.exchanges==1 && f.enables==1);
    CHECK(f.authority==(0x80000000U|(1U<<5)));gn::Weak actual{};CHECK(f.value(f.source+0x1F0,actual));CHECK(actual==f.device);
    CHECK(f.bind()==sh::Bound::ready && f.exchanges==1);
    Fixture stale;stale.put(stale.source+0x1F0,gn::Weak{999,stale.logicalDevice.handle});CHECK(stale.bind()==sh::Bound::changed);
    Fixture absent;absent.put(absent.source+0x1F0,gn::Weak{});CHECK(absent.bind()==sh::Bound::changed);
    Fixture foreign;foreign.put(foreign.logical,0x12345678U);CHECK(foreign.bind()==sh::Bound::foreign);CHECK(!foreign.exchanges && !foreign.enables);
    Fixture displaced;displaced.put(displaced.logicalRow+0xD0,5U);CHECK(displaced.bind()==sh::Bound::foreign);CHECK(!displaced.exchanges);
    Fixture recycled;++recycled.device.serial;CHECK(sh::bind(recycled,recycled,recycled.image,recycled.source,f.target())==sh::Bound::invalid);CHECK(!recycled.exchanges);
    Fixture wrongOwner;wrongOwner.put(wrongOwner.physical+0x2C,999U);CHECK(wrongOwner.bind()==sh::Bound::invalid);
    Fixture retired;retired.put(retired.row+4,4U);CHECK(retired.bind()==sh::Bound::invalid && !retired.enables);
    Fixture wrongSource;wrongSource.put(wrongSource.source,0x8153C14BU);CHECK(wrongSource.bind()==sh::Bound::invalid);
    Fixture raced;raced.race=true;CHECK(raced.bind()==sh::Bound::raced && !raced.enables);
}
}
