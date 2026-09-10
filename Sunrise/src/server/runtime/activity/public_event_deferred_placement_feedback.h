#pragma once
#include "../../../state/activity/lifecycle_generation.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace sunrise::server::runtime::activity::public_event::deferred_placement {
using Owner=state::activity::ActivityInstanceKey;
inline constexpr std::uint32_t kProducerRva=0x9EFFC0;
inline constexpr std::size_t kComponentBytes=0x448,kAuthorityBytes=0x170;
struct Ticket final {
    Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::uint32_t registry{},definition{},generation{};
    std::int64_t definitionOffset{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    // This bounded contract has one authored visual, selector0, no dynamic
    // override and a positive generation. It does not extend rally semantics.
    std::uint32_t entityDefinition{};
    std::uint64_t pointGuid{};
    std::uint32_t pointComponent{};
    std::int64_t pointInterfaceOffset{};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] constexpr bool tag(std::uint32_t value) noexcept {
    return value && value!=UINT32_MAX && value!=0x811C9DC5;
}
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return t.owner && t.boot && t.definitionRevision && t.selectionRevision && t.event
        && tag(t.registry) && tag(t.definition) && tag(t.entityDefinition) && t.pointGuid
        && t.generation && t.generation<=0x7FFFFFFFU && t.definitionOffset>0
        && t.definitionOffset<0x2000000 && t.slot<=32767 && t.bubble<=63;
}
template<class T> [[nodiscard]] T field(std::span<const std::byte> bytes,std::size_t at) noexcept {
    T out{};if(at<=bytes.size() && sizeof out<=bytes.size()-at)std::memcpy(&out,bytes.data()+at,sizeof out);return out;
}
struct Source final {
    std::uint32_t member{UINT32_MAX},componentLink{UINT32_MAX};
    std::int64_t offset{-1};
    friend bool operator==(const Source&,const Source&)=default;
};
struct Capture final {
    Ticket ticket{};Source source{};
    std::uint64_t sequence{};
    std::uint32_t producerRva{},resolvedChild{UINT32_MAX};
    std::span<const std::byte> before,after,authorityObject,authorityBody,definition,visual,entity;
    // The wrapper resolves the native common-pool self and full salted weak
    // child, with stable definition/authority/pool identity bracketing reads.
    // These booleans record successful operations, never publication readiness.
    bool originalForwarded{},stableNativeIdentity{},weakChildResolved{},originalCreated{};
};
struct Observation final {Ticket ticket{};Source source{};std::uint64_t sequence{},weakChild{};std::uint32_t child{UINT32_MAX};};
enum class Result:std::uint8_t {accepted,identity,definition,authority,body,generation,child};
[[nodiscard]] inline bool authored_body(std::span<const std::byte> b,std::uint32_t generation) noexcept {
    return b.size()==kAuthorityBytes && field<std::uint32_t>(b,0)==generation && field<std::int32_t>(b,4)==0
        && field<std::uint8_t>(b,8)==1 && field<std::uint8_t>(b,9)==0 && field<std::int32_t>(b,0xC)==0
        && field<std::uint32_t>(b,0x10)==0x811C9DC5 && field<std::uint8_t>(b,0x14)==0xFF
        && field<std::uint16_t>(b,0x16)==0xFFFF && field<std::uint32_t>(b,0x20)==0
        && field<std::uint32_t>(b,0x24)==0 && field<std::uint32_t>(b,0x28)==0
        && field<std::uint8_t>(b,0x30)==0 && field<std::uint32_t>(b,0x40)==0;
}
// Pure qualification of original9EFFC0 creation, called by deferred9F2F30. The
// already applied authority must match before creation; it commits that positive
// generation only after the native factory succeeds. No death, reissue, point
// interface, enemy readiness or event progression is inferred from this receipt.
[[nodiscard]] inline Result qualify(const Ticket& expected,const Capture& c,Observation& out) noexcept {
    out={};
    if(!valid(expected) || c.ticket!=expected || !c.sequence || c.producerRva!=kProducerRva
        || !c.originalForwarded || !c.originalCreated || !c.stableNativeIdentity || !c.weakChildResolved
        || c.source.member==UINT32_MAX || c.source.componentLink==UINT32_MAX
        || (c.source.member&0x1FFFU)!=(c.source.componentLink&0x1FFFU)
        || c.source.offset<0 || c.source.offset>=0x2000000
        || c.before.size()!=kComponentBytes || c.after.size()!=kComponentBytes
        || c.authorityObject.size()!=0x70 || c.definition.size()!=0x98 || c.visual.size()!=144
        || c.entity.size()!=0x98)return Result::identity;
    const auto b=c.before,a=c.after,d=c.definition,o=c.authorityObject;
    if(field<std::uint32_t>(a,0)!=expected.definition || field<std::uint32_t>(a,4)!=0x80809928
        || field<std::int64_t>(a,8)!=expected.definitionOffset || !std::equal(a.begin(),a.begin()+16,b.begin())
        || field<std::uint32_t>(a,0x20)!=c.source.componentLink || field<std::uint32_t>(b,0x20)!=c.source.componentLink
        || field<std::uint32_t>(a,0x170)==UINT32_MAX || field<std::uint32_t>(a,0x170)!=field<std::uint32_t>(b,0x170))return Result::identity;
    if(field<std::uint32_t>(d,0)!=expected.definition || field<std::uint32_t>(d,4)!=0x80809927
        || field<std::uint32_t>(d,0x30)!=expected.registry || field<std::uint8_t>(d,0x34)!=4
        || field<std::uint16_t>(d,0x36)!=expected.slot || field<std::uint32_t>(d,0x38)!=expected.bubble
        || field<std::uint32_t>(d,0x44)!=0x8080992E || field<std::uint32_t>(d,0x48)!=0x8080992F
        || field<std::uint64_t>(d,0x58)!=1 || field<std::uint8_t>(d,0x94)!=1
        || field<std::uint32_t>(c.visual,0)!=expected.entityDefinition
        || field<std::uint64_t>(c.visual,0x70)!=expected.pointGuid)return Result::definition;
    if(field<std::uint32_t>(o,0)!=expected.registry || field<std::uint8_t>(o,4)!=4
        || field<std::uint16_t>(o,6)!=expected.slot || field<std::uint32_t>(o,0xC)!=0x8080992F
        || field<std::uint8_t>(o,0x18)!=0 || field<std::uint32_t>(o,0x68)!=expected.bubble)return Result::authority;
    if(!authored_body(c.authorityBody,expected.generation)
        || !std::equal(c.authorityBody.begin(),c.authorityBody.end(),b.begin()+0x180)
        || !std::equal(c.authorityBody.begin(),c.authorityBody.end(),a.begin()+0x180))return Result::body;
    const auto prior=field<std::int32_t>(b,0x2F0),current=field<std::int32_t>(a,0x2F0);
    if(prior>=static_cast<std::int32_t>(expected.generation) || current!=static_cast<std::int32_t>(expected.generation)
        || field<std::uint32_t>(a,0x2F8)!=0 || !(field<std::uint32_t>(a,0x2FC)&1U))return Result::generation;
    const auto weak=field<std::uint64_t>(a,0x440);
    if(c.resolvedChild==UINT32_MAX || field<std::uint32_t>(a,0x444)!=c.resolvedChild
        || field<std::uint32_t>(c.entity,0x0C)!=c.resolvedChild || (field<std::uint32_t>(c.entity,4)&4U)
        || field<std::uint64_t>(c.entity,0x90)!=expected.pointGuid)return Result::child;
    // The first run starts absent. The bridge retains its one accepted receipt;
    // another deferred tick at the same generation does not call this creator.
    // Replacing an extant child needs a separate retirement contract.
    if(field<std::uint32_t>(b,0x444)!=UINT32_MAX)return Result::child;
    out={expected,c.source,c.sequence,weak,c.resolvedChild};return Result::accepted;
}
} // namespace sunrise::server::runtime::activity::public_event::deferred_placement
