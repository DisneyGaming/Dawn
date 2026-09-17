#pragma once
#include "adventure_cue_feedback.h"
#include "../../../middleware/bap/activity_message/native/public_event_engagement_authority.h"

namespace dawn::server::runtime::activity::public_event::engagement_feedback {
namespace wire=middleware::bap::activity_message::native::engagement;
using Owner=state::activity::ActivityInstanceKey;
using Source=adventure::cue_feedback::Source;
using adventure::cue_feedback::field;
inline constexpr std::uint32_t kProducerRva=0x9F1820;
inline constexpr std::size_t kComponentBytes=0x2E0;
struct Ticket final {
    Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::uint32_t definition{};
    std::int64_t definitionOffset{};
    wire::Request request{};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return t.owner && t.boot && t.definitionRevision && t.selectionRevision && t.event
        && t.definition && t.definition!=UINT32_MAX && t.definition!=0x811C9DC5
        && t.definitionOffset>0 && t.definitionOffset<0x2000000 && wire::valid(t.request);
}
struct Capture final {
    Ticket ticket{};Source source{};
    std::uint64_t sequence{};
    std::uint32_t producerRva{};
    std::span<const std::byte> prior,applied,incoming,authorityObject;
    wire::IdentifierEncoding nativeIdentifierEncoding{wire::IdentifierEncoding::unknown};
    bool originalForwarded{};
};
struct Observation final {Ticket ticket{};Source source{};std::uint64_t sequence{};};
enum class Result:std::uint8_t {accepted,identity,body,staleGeneration,localParticipants};
// The observer must qualify the common-pool self, authored definition, native
// authority and codec gate before/after the unchanged original callback. This
// pure stage verifies its immutable ticket and copied data without native calls.
[[nodiscard]] inline Result qualify(const Ticket& expected,const Capture& c,Observation& out) noexcept {
    out={};
    if(!valid(expected) || c.ticket!=expected || !c.sequence || !c.originalForwarded
        || c.producerRva!=kProducerRva || c.source.member==UINT32_MAX || c.source.offset<0
        || c.source.offset>=0x2000000
        || (expected.request.collection==wire::Collection::explicitParticipants
            && c.nativeIdentifierEncoding!=expected.request.identifierEncoding)
        || c.prior.size()!=kComponentBytes || c.applied.size()!=kComponentBytes || c.authorityObject.size()!=0x70)return Result::identity;
    const auto before=c.prior,after=c.applied,a=c.authorityObject;
    if(field<std::uint32_t>(after,0)!=expected.definition || field<std::uint32_t>(after,4)!=0x808094EF
        || field<std::int64_t>(after,8)!=expected.definitionOffset
        || !std::equal(before.begin(),before.begin()+16,after.begin())
        || field<std::uint32_t>(before,0x20)!=field<std::uint32_t>(after,0x20)
        || field<std::uint32_t>(after,0x170)==UINT32_MAX
        || field<std::uint32_t>(before,0x170)!=field<std::uint32_t>(after,0x170)
        || field<std::uint32_t>(a,0)!=expected.request.registry || field<std::uint8_t>(a,4)!=70
        || field<std::uint16_t>(a,6)!=expected.request.slot || field<std::uint32_t>(a,0xC)!=0x808094F1
        || field<std::uint8_t>(a,0x18)!=0 || field<std::uint32_t>(a,0x68)!=expected.request.scope)return Result::identity;
    if(!wire::matches_fields(c.incoming,expected.request)
        || !std::equal(c.incoming.begin(),c.incoming.end(),after.begin()+0x180))return Result::body;
    // Original9F1820 copies older bodies while retaining the newer committed
    // signed generation. Cache equality must never accept that stale state.
    const auto priorGeneration=field<std::int16_t>(before,0x2D4);
    if(priorGeneration>expected.request.generation || field<std::int16_t>(after,0x2D4)!=expected.request.generation)return Result::staleGeneration;
    const auto priorCount=field<std::uint32_t>(before,0x250);
    const auto expectedCount=priorGeneration<expected.request.generation?0:priorCount;
    if((expected.request.collection==wire::Collection::none && field<std::uint32_t>(after,0x250)!=0)
        || priorCount>16 || field<std::uint32_t>(after,0x250)!=expectedCount
        || !std::equal(before.begin()+0x254,before.begin()+0x2D4,after.begin()+0x254))return Result::localParticipants;
    out={expected,c.source,c.sequence};return Result::accepted;
}
}
