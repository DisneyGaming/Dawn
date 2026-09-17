#pragma once
#include "adventure_cue_feedback.h"
#include "../../../middleware/bap/activity_message/native/public_event_participant_authority.h"

namespace dawn::server::runtime::activity::public_event::participant_feedback {
namespace wire=middleware::bap::activity_message::native::event_participant;
namespace identifiers=middleware::bap::activity_message::native::engagement;
namespace cue=middleware::bap::activity_message::native::cue;
using Owner=state::activity::ActivityInstanceKey;
using Source=adventure::cue_feedback::Source;
using adventure::cue_feedback::field;
inline constexpr std::uint32_t kProducerRva=0xBF5AA0;
inline constexpr std::size_t kComponentBytes=0x1A8;
struct LocalIdentity final {
    std::uint64_t entity{};
    identifiers::IdentifierEncoding encoding{identifiers::IdentifierEncoding::unknown};
    friend bool operator==(const LocalIdentity&,const LocalIdentity&)=default;
};
[[nodiscard]] inline bool valid(const LocalIdentity& i) noexcept {
    return i.entity && i.entity!=UINT64_MAX
        && (i.encoding==identifiers::IdentifierEncoding::integer
            || i.encoding==identifiers::IdentifierEncoding::byte_array);
}
struct Ticket final {
    Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::uint32_t definition{};
    std::int64_t definitionOffset{};
    wire::Request request{};
    std::uint32_t nativeScope{UINT32_MAX};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return t.owner && t.boot && t.definitionRevision && t.selectionRevision && t.event
        && t.definition && t.definition!=UINT32_MAX && t.definition!=cue::kAbsent
        && t.definitionOffset>0 && t.definitionOffset<0x2000000 && wire::valid(t.request)
        && valid(LocalIdentity{t.request.entity,t.request.identifierEncoding})
        && (t.nativeScope==UINT32_MAX || t.nativeScope<=63);
}
struct Capture final {
    Ticket ticket{}; Source source{};
    std::uint64_t sequence{}; std::uint32_t producerRva{};
    std::span<const std::byte> prior,applied,incoming,authorityObject;
    LocalIdentity localBefore{},localAfter{};
    bool originalForwarded{};
};
struct Observation final {Ticket ticket{};Source source{};std::uint64_t sequence{};};
enum class Result:std::uint8_t {accepted,identity,body,localParticipant};
[[nodiscard]] inline Result qualify(const Ticket& expected,const Capture& c,Observation& out) noexcept {
    out={};
    if(!valid(expected) || c.ticket!=expected || !c.sequence || !c.originalForwarded
        || c.producerRva!=kProducerRva || c.source.member==UINT32_MAX || c.source.offset<0
        || c.source.offset>=0x2000000 || c.prior.size()!=kComponentBytes
        || c.applied.size()!=kComponentBytes || c.authorityObject.size()!=0x70)return Result::identity;
    const auto before=c.prior,after=c.applied,a=c.authorityObject;
    if(field<std::uint32_t>(after,0)!=expected.definition || field<std::uint32_t>(after,4)!=0x80804F4A
        || field<std::int64_t>(after,8)!=expected.definitionOffset
        || !std::equal(before.begin(),before.begin()+16,after.begin())
        || field<std::uint32_t>(before,0x20)!=field<std::uint32_t>(after,0x20)
        || field<std::uint32_t>(after,0x170)==UINT32_MAX
        || field<std::uint32_t>(before,0x170)!=field<std::uint32_t>(after,0x170)
        || field<std::uint32_t>(a,0)!=expected.request.registry || field<std::uint8_t>(a,4)!=71
        || field<std::uint16_t>(a,6)!=expected.request.slot || field<std::uint32_t>(a,0xC)!=0x80804F57
        || field<std::uint8_t>(a,0x18)!=0 || field<std::uint32_t>(a,0x68)!=expected.nativeScope)return Result::identity;
    if(!wire::matches_fields(c.incoming,expected.request)
        || !std::equal(c.incoming.begin(),c.incoming.end(),after.begin()+0x180))return Result::body;
    const LocalIdentity expectedLocal{expected.request.entity,expected.request.identifierEncoding};
    if(c.localBefore!=expectedLocal || c.localAfter!=expectedLocal
        || field<std::uint8_t>(after,0x199)!=1)return Result::localParticipant;
    // This is the original's local presentation classification, not a combat
    // participation receipt or proof that the native CUI displayed its banner.
    out={expected,c.source,c.sequence};return Result::accepted;
}
}
