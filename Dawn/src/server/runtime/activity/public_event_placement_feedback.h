#pragma once
#include "public_event_service.h"
#include <cstring>
#include <span>

namespace dawn::server::runtime::activity::public_event::placement_feedback {
// Native type-4 apply9F19F0 adopts8080992F. Creator9EFFC0 commits generation
// at+2F0, selector+2F8 and weak object+440 only after successful9EFBC0 creation.
// The local rally placement's definition+94 is0: sense producer9F0750 exits
// in that mode, so sense+5 MUST NOT be required/interpreted for this binding.
inline constexpr std::uint32_t kProducerRva=0x9F19F0;
inline constexpr std::size_t kComponentBytes=0x448,kAuthorityBytes=0x170,kSenseBytes=0x150;
struct Definition final {
    std::int64_t nativeDefinitionOffset{};
    std::uint8_t authoredVisualCount{};
    // Explicit destination capability: the sole placed entity has one authored
    // interaction controller, absent scoped override and zero command revision.
    bool enableInteractionAfterPlacement{};
    // Optional authored use controller; zero leaves this a placement-only lease.
    std::uint32_t interactionDefinition{};
    std::int64_t interactionOffset{};
};
struct Ticket final {
    Lease lease{};
    coo::Asset asset{};
    coo::Token token{};
    // Legacy field name: native authority+68 is authored bubble scope, not a
    // network endpoint. It must equal this ticket's admitted bubble.
    std::uint32_t bubble{},authorityOwner{};
    Definition definition{};
};
struct SourceReference final { std::uint32_t member{UINT32_MAX};std::int64_t offset{}; };
// The capture owner must copy these bytes after the unmodified native apply
// returns, while retaining the ticket acquired before the callback. Resolve the
// full salted source/entity handles through the native resolver at capture time.
// Never fill a delayed capture with a newly looked-up current owner/ticket.
struct Capture final {
    Ticket ticket{};
    std::span<const std::byte> component,authorityObject,authorityBody,senseBody;
    SourceReference resolvedSource{};
    std::uint32_t resolvedEntity{UINT32_MAX},entityFlags{},producerRva{};
    std::uint64_t sequence{};
    std::span<const std::byte> priorComponent;
    std::uint32_t priorResolvedEntity{UINT32_MAX},priorEntityFlags{};
};
struct Observation final {
    Receipt receipt{};
    SourceReference source{};
    std::uint32_t entity{UINT32_MAX},selector{},authorityOwner{};
    std::uint64_t sequence{};
};
template<class T> [[nodiscard]] T field(std::span<const std::byte> bytes,std::size_t at) noexcept {
    T value{};if(at<=bytes.size() && sizeof value<=bytes.size()-at)std::memcpy(&value,bytes.data()+at,sizeof value);
    return value;
}
[[nodiscard]] inline bool same(const Ticket& a,const Ticket& b) noexcept {
    return a.lease==b.lease && a.asset==b.asset && a.token==b.token && a.bubble==b.bubble
        && a.authorityOwner==b.authorityOwner
        && a.definition.nativeDefinitionOffset==b.definition.nativeDefinitionOffset
        && a.definition.authoredVisualCount==b.definition.authoredVisualCount
        && a.definition.enableInteractionAfterPlacement==b.definition.enableInteractionAfterPlacement
        && a.definition.interactionDefinition==b.definition.interactionDefinition
        && a.definition.interactionOffset==b.definition.interactionOffset;
}
// Pure qualification: no native calls, pointer dereferences, timers or writes.
// This proves native placement only. It does not prove the flag was used, start
// the encounter, interpret the public-event sensor, or establish retirement.
[[nodiscard]] inline bool qualify(const Ticket& expected,const Capture& capture,Observation& output) noexcept {
    if(!expected.lease.valid() || expected.asset.type!=4 || !expected.asset.registry || !expected.asset.definition
        || expected.token.run!=expected.lease.event || !expected.token.incarnation || expected.bubble>63
        || expected.authorityOwner!=expected.bubble
        || expected.definition.nativeDefinitionOffset<=0 || !expected.definition.authoredVisualCount
        || expected.definition.authoredVisualCount>64 || !same(expected,capture.ticket)
        || capture.producerRva!=kProducerRva || !capture.sequence
        || capture.component.size()!=kComponentBytes || capture.authorityObject.size()!=0x70
        || capture.authorityBody.size()!=kAuthorityBytes || capture.senseBody.size()!=kSenseBytes
        || capture.priorComponent.size()!=kComponentBytes) return false;
    const auto c=capture.component,a=capture.authorityObject,b=capture.authorityBody,s=capture.senseBody;
    const auto prior=capture.priorComponent;
    if(field<std::uint32_t>(c,0)!=expected.asset.definition || field<std::uint32_t>(c,4)!=0x80809928
        || field<std::int64_t>(c,8)!=expected.definition.nativeDefinitionOffset
        || field<std::uint32_t>(c,0x164)!=0x80809927 || capture.resolvedSource.member==UINT32_MAX
        || capture.resolvedSource.offset<0 || capture.resolvedSource.offset>0x2000000
        || field<std::uint32_t>(c,0x160)!=capture.resolvedSource.member
        || field<std::int64_t>(c,0x168)!=capture.resolvedSource.offset
        || field<std::uint32_t>(a,0)!=expected.asset.registry || field<std::uint8_t>(a,4)!=4
        || field<std::uint16_t>(a,6)!=expected.asset.slot || field<std::uint32_t>(a,0xC)!=0x8080992F
        || field<std::uint8_t>(a,0x18)!=0 || field<std::uint32_t>(a,0x68)!=expected.authorityOwner
        || std::memcmp(c.data()+0x180,b.data(),kAuthorityBytes)!=0
        || std::memcmp(c.data()+0x2F0,s.data(),kSenseBytes)!=0) return false;
    // Both snapshots must belong to the same full native component and authority.
    if(std::memcmp(c.data(),prior.data(),16)!=0 || std::memcmp(c.data()+0x160,prior.data()+0x160,0x14)!=0)return false;
    // Decode of shared placement::write_active, retaining its signed sentinels.
    // The raw wire generation0 is INT_MIN; it is not a host event counter.
    if(field<std::uint32_t>(b,0)!=0x80000000U || field<std::uint32_t>(b,4)!=0x80000001U
        || field<std::uint8_t>(b,8)!=1 || field<std::uint8_t>(b,9)!=1
        || field<std::uint32_t>(b,0xC)!=0x80000000U || field<std::uint32_t>(b,0x10)!=0x811C9DC5
        || field<std::uint8_t>(b,0x14)!=0xFF || field<std::uint16_t>(b,0x16)!=0xFFFF
        || field<std::uint32_t>(b,0x20)!=0 || field<std::uint32_t>(b,0x24)!=0
        || field<std::uint32_t>(b,0x28)!=0 || field<std::uint8_t>(b,0x30)!=0
        || field<std::uint32_t>(b,0x40)!=1) return false;
    const auto selector=field<std::uint32_t>(s,8);
    if(selector>=expected.definition.authoredVisualCount || capture.resolvedEntity==UINT32_MAX
        || field<std::uint32_t>(c,0x444)!=capture.resolvedEntity || (capture.entityFlags&4U)) return false;
    const bool priorPresent=capture.priorResolvedEntity!=UINT32_MAX && !(capture.priorEntityFlags&4U);
    if(priorPresent) {
        // Local9F19F0 retains a living object. Its committed generation belongs
        // to that creation, even after a different incoming authority is adopted.
        if(capture.priorResolvedEntity!=field<std::uint32_t>(prior,0x444)
            || capture.priorResolvedEntity!=capture.resolvedEntity
            || field<std::uint64_t>(prior,0x440)!=field<std::uint64_t>(c,0x440)
            || field<std::uint32_t>(prior,0x2F0)!=field<std::uint32_t>(s,0)
            || field<std::uint32_t>(prior,0x2F8)!=selector)return false;
    } else {
        // Original9F19F0 creates BEFORE copying the incoming body.9EFFC0 commits
        // the previously prepared generation, not the incoming wire generation.
        if((capture.priorResolvedEntity==UINT32_MAX && field<std::uint32_t>(prior,0x444)!=UINT32_MAX)
            || field<std::uint32_t>(prior,0x180)!=field<std::uint32_t>(s,0))return false;
    }
    if(!(field<std::uint32_t>(s,12+(selector/32)*4)&(1U<<(selector%32))))return false;
    output={{expected.lease,Stage::rally,expected.asset,{expected.token,coo::Milestone::nativeReady}},
        capture.resolvedSource,capture.resolvedEntity,selector,expected.authorityOwner,capture.sequence};
    return true;
}
} // namespace dawn::server::runtime::activity::public_event::placement_feedback
