#pragma once
#include <cstdint>
#include <array>

namespace dawn::client::hooks::bootflow::deadly_trial_lifetime {
enum class Result { unavailable, unchanged, repaired };
struct Ref { std::uint32_t handle{UINT32_MAX},kind{};std::int64_t offset{}; };
struct Identity { std::uint32_t key{};std::uint8_t type{},pad{};std::int16_t slot{}; };

// A region retirement can leave the retained global runtime without its owner
// and sync record. Reconnect those existing objects before native packet decode.
// Never write the lifetime body: the host's real state-6 packet must apply it.
template<class Read,class Native>
Result repair(Read& read,Native& native,std::uintptr_t roster,std::uint32_t expectedScenario=0x80B2E043U) noexcept {
    // Only the two missions with verified global lifetime ownership use this repair.
    if(expectedScenario!=0x80B2E043U && expectedScenario!=0x80B4206AU) {return Result::unavailable;}
    std::uint32_t owner{},scenario{};std::uintptr_t activity{},context{};
    if(!roster || !read.value(roster+0x820,owner) || !read.resolve(owner,activity)
        || activity+0x28!=roster || !read.value(activity+0x24,scenario) || scenario!=expectedScenario
        || !(context=native.context())) { return Result::unavailable; }
    std::uint32_t count{};
    if(!read.value(context+8,count) || count>128) { return Result::unavailable; }
    std::uintptr_t ownerField{};
    for(std::uint32_t i=0;i<count;++i) {
        std::array<std::uint32_t,6> row{};
        if(!read.value(context+0xC+i*24,row)) { return Result::unavailable; }
        if(row[2]!=0x4786C0E0U) { continue; }
        if(ownerField || row[0]!=21 || row[3]!=0x80FEB3DCU
            || (row[4]!=UINT32_MAX && row[4]!=owner)) { return Result::unavailable; }
        ownerField=context+0x1C+i*24;
    }
    if(!ownerField) { return Result::unavailable; }
    // Lifetime services have no actor self-handle at +0x24 (zero in the live object).
    // Validate the native lookup, resolved definition and salted sync record instead.
    Ref ref{},definition{};std::uintptr_t lifetime{};std::uint32_t sync{},oldOwner{};
    if(!native.lookup(Identity{0x4786C0E0U,17,0,3},ref) || ref.kind!=0x80809915U || ref.offset!=0
        || !read.resolve(ref.handle,lifetime) || !read.value(lifetime,definition)
        || definition.handle!=0x80FE12B6U || definition.kind!=0x80809916U || definition.offset!=0x718
        || !read.value(lifetime+0x170,sync) || !read.value(ownerField,oldOwner)
        || (oldOwner!=UINT32_MAX && oldOwner!=owner)) { return Result::unavailable; }
    if(oldOwner==owner && sync!=UINT32_MAX) { return Result::unchanged; }
    std::uintptr_t pool{},elements{};std::uint32_t capacity{},limit{},stride{},saltOffset{},tag{};
    if(!read.value(roster+0x808,pool) || !read.value(pool+8,elements)
        || !read.value(pool+0x14,capacity) || !read.value(pool+0x18,limit)
        || !read.value(pool+0x1C,saltOffset) || !read.value(pool+0x20,stride)
        || !read.value(pool+0x34,tag) || capacity>4096 || limit>capacity
        || stride!=0x88 || saltOffset!=0x80 || (tag&0x40000000U)) { return Result::unavailable; }
    std::uint32_t selected{UINT32_MAX};
    for(std::uint32_t i=0;i<limit;++i) {
        if(!native.allocated(pool,static_cast<std::uint16_t>(i))) { continue; }
        const auto address=elements+static_cast<std::uintptr_t>(i)*stride;
        Identity identity{};
        if(!read.value(address,identity)) { return Result::unavailable; }
        if(identity.key!=0x4786C0E0U || identity.type!=17 || identity.slot!=3) { continue; }
        std::uint32_t schema{},salt{};std::uintptr_t resolved{};
        if(selected!=UINT32_MAX || !read.value(address+12,schema) || schema!=0x8080991AU
            || !read.value(address+saltOffset,salt)) { return Result::unavailable; }
        selected=((((salt&255U)<<10)|(tag&1023U))<<13)|i;
        if(!read.resolve(selected,resolved) || resolved!=address) { return Result::unavailable; }
    }
    if(selected==UINT32_MAX || (sync!=UINT32_MAX && sync!=selected)) { return Result::unavailable; }
    // All identities are checked before either write. CAS cannot replace another
    // live owner or a newly assigned sync record during a retirement boundary.
    if(!native.assign(ownerField,oldOwner,owner)
        || !native.assign(lifetime+0x170,sync,selected)) { return Result::unavailable; }
    return Result::repaired;
}
bool install() noexcept;
void quiesce() noexcept;
bool uninstall() noexcept;
}
