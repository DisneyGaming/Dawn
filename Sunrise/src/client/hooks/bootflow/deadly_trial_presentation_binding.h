#pragma once
#include "deadly_trial_lifetime.h"

namespace sunrise::client::hooks::bootflow::deadly_trial_lifetime {
// Captured in the failed Trial run: the shared root and both presentation
// services survived public-activity retirement with their associations cleared.
// Reconnect existing authority; native packet apply and dispatch still own data.
template<class Read,class Native>
Result repair_presentation(Read& read,Native& native,std::uintptr_t roster) noexcept {
    std::uint32_t owner{},scenario{};std::uintptr_t activity{},context{};
    if(!roster || !read.value(roster+0x820,owner) || !read.resolve(owner,activity)
        || activity+0x28!=roster || !read.value(activity+0x24,scenario) || scenario!=0x80B2E043U
        || !(context=native.context()))return Result::unavailable;
    std::uint32_t count{},oldOwner{};std::uintptr_t ownerField{};
    if(!read.value(context+8,count) || count>128)return Result::unavailable;
    for(std::uint32_t i=0;i<count;++i) {
        std::array<std::uint32_t,6> row{};
        if(!read.value(context+0xC+i*24,row))return Result::unavailable;
        if(row[2]!=0xC9BC773AU)continue;
        if(ownerField || row[0]!=3 || row[3]!=0x80B2ED51U
            || (row[4]!=UINT32_MAX && row[4]!=owner))return Result::unavailable;
        ownerField=context+0x1C+i*24;oldOwner=row[4];
    }
    if(!ownerField)return Result::unavailable;
    struct Binding {std::uint8_t type;std::int16_t slot;std::uint32_t definition,kind,schema;std::int64_t offset;};
    constexpr std::array bindings{
        Binding{53,2,0x80B2E709U,0x80804F4BU,0x80804F77U,0x1408},
        Binding{68,0,0x80B2E706U,0x80804F53U,0x80804F67U,0xB88}};
    struct Link {std::uintptr_t runtime{};std::uint32_t previous{},selected{UINT32_MAX};Ref ref{};};
    std::array<Link,bindings.size()> links{};
    bool missing=oldOwner!=owner;
    for(std::size_t i=0;i<bindings.size();++i) {
        const auto& b=bindings[i];auto& link=links[i];Ref definition{};std::uint32_t self{};
        if(!native.lookup(Identity{0xC9BC773AU,b.type,0,b.slot},link.ref)
            || link.ref.kind!=b.kind || link.ref.offset!=0 || !read.resolve(link.ref.handle,link.runtime)
            || !read.value(link.runtime,definition) || definition.handle!=b.definition
            || definition.kind!=b.kind+1 || definition.offset!=b.offset
            || !read.value(link.runtime+0x48,self) || self!=link.ref.handle
            || !read.value(link.runtime+0x170,link.previous))return Result::unavailable;
        missing|=link.previous==UINT32_MAX;
    }
    if(!missing)return Result::unchanged;
    std::uintptr_t pool{},elements{};std::uint32_t capacity{},limit{},stride{},saltOffset{},tag{};
    if(!read.value(roster+0x808,pool) || !read.value(pool+8,elements)
        || !read.value(pool+0x14,capacity) || !read.value(pool+0x18,limit)
        || !read.value(pool+0x1C,saltOffset) || !read.value(pool+0x20,stride)
        || !read.value(pool+0x34,tag) || capacity>4096 || limit>capacity
        || stride!=0x88 || saltOffset!=0x80 || (tag&0x40000000U))return Result::unavailable;
    for(std::uint32_t i=0;i<limit;++i) {
        if(!native.allocated(pool,static_cast<std::uint16_t>(i)))continue;
        const auto address=elements+static_cast<std::uintptr_t>(i)*stride;Identity identity{};
        if(!read.value(address,identity))return Result::unavailable;
        if(identity.key!=0xC9BC773AU)continue;
        for(std::size_t j=0;j<bindings.size();++j) {
            const auto& b=bindings[j];auto& link=links[j];
            if(identity.type!=b.type || identity.slot!=b.slot)continue;
            std::uint32_t schema{},salt{};std::uintptr_t resolved{};
            if(link.selected!=UINT32_MAX || !read.value(address+12,schema) || schema!=b.schema
                || !read.value(address+saltOffset,salt))return Result::unavailable;
            link.selected=((((salt&255U)<<10)|(tag&1023U))<<13)|i;
            if(!read.resolve(link.selected,resolved) || resolved!=address)return Result::unavailable;
        }
    }
    for(const auto& link:links) {
        if(link.selected==UINT32_MAX || (link.previous!=UINT32_MAX && link.previous!=link.selected))return Result::unavailable;
    }
    // Validate the current owner and all records before changing an association.
    if(!native.assign(ownerField,oldOwner,owner))return Result::unavailable;
    for(const auto& link:links) {
        if(link.previous!=link.selected && !native.assign(link.runtime+0x170,link.previous,link.selected))return Result::unavailable;
    }
    return Result::repaired;
}
}
