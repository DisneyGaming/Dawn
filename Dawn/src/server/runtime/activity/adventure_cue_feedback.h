#pragma once
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../middleware/bap/activity_message/native/adventure_cue_authority.h"
#include <algorithm>

namespace dawn::server::runtime::activity::adventure::cue_feedback {
namespace wire=middleware::bap::activity_message::native::cue;
using Owner=state::activity::ActivityInstanceKey;
inline constexpr std::uint32_t kProducerRva=0x1009C00;
// Two authored layouts verified through original1008B40/137E6F0. The enum
// selects an exact receipt contract, never an override of native presentation.
enum class Presentation:std::uint8_t {activityObjective,authoredProgress};
// Native authored progress display at manager entry +74. The traveling-Vex
// objective uses a percentage; the key objective uses a count (0 of 4).
enum class ProgressFormat:std::uint8_t {none=0,count=1,percent=2};
// Original1009740: definition+58 selects the native readiness predicate route.
// Activity objectives retain0; the authored Crossroads progress layout uses1.
[[nodiscard]] constexpr bool matches_predicate(Presentation p,std::uint8_t native) noexcept {
    return (p==Presentation::activityObjective && native==0)
        || (p==Presentation::authoredProgress && native==1);
}
struct Ticket final {
    Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{};
    std::int16_t activity{-1};
    std::uint32_t definition{},nativeScope{UINT32_MAX};
    std::uint32_t table{},stringBank{},title{},detail{};
    std::int64_t definitionOffset{};
    wire::Request request{};
    Presentation presentation{Presentation::activityObjective};
    std::uint32_t progressLabel{wire::kAbsent};
    // A local registry may contain authored global components. Its admission
    // scope is independent of the native definition/authority scope above.
    std::uint32_t admissionScope{UINT32_MAX};
    ProgressFormat progressFormat{ProgressFormat::percent};
    // Exact native mode0 is a valid incoming public-event presentation. It is
    // not a fulfilled type70 readiness predicate or proof of participation.
    bool incoming{};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return t.owner && t.boot && t.definitionRevision && t.selectionRevision && t.activity>=0
        && t.definition && t.definition!=UINT32_MAX && t.definition!=wire::kAbsent
        && t.table && t.table!=UINT32_MAX && t.stringBank && t.stringBank!=UINT32_MAX
        && t.title && t.title!=wire::kAbsent && t.detail && t.detail!=wire::kAbsent
        && t.definitionOffset>0 && t.definitionOffset<0x2000000 && wire::valid(t.request)
        && (t.nativeScope==UINT32_MAX || t.nativeScope<=63) && t.admissionScope==t.request.scope
        && (!t.incoming || (t.presentation==Presentation::authoredProgress && t.request.readiness.type==70))
        && ((t.presentation==Presentation::activityObjective && t.progressLabel==wire::kAbsent)
            || (t.presentation==Presentation::authoredProgress
                && ((t.progressFormat==ProgressFormat::none && t.progressLabel==wire::kAbsent)
                    || (t.progressLabel && t.progressLabel!=wire::kAbsent && t.progressLabel!=UINT32_MAX
                        && (t.progressFormat==ProgressFormat::count || t.progressFormat==ProgressFormat::percent)))));
}
template<class T> [[nodiscard]] T field(std::span<const std::byte> b,std::size_t at) noexcept {
    T value{};if(at<=b.size() && sizeof(T)<=b.size()-at)std::memcpy(&value,b.data()+at,sizeof(T));return value;
}
struct Source final {
    std::uint32_t member{UINT32_MAX};std::int64_t offset{-1};
    friend bool operator==(const Source&,const Source&)=default;
};
struct Capture final {
    Ticket ticket{};
    Source source{};
    std::uint64_t sequence{};
    std::uint32_t producerRva{};
    std::span<const std::byte> incoming,prior,applied,entry;
    std::uint64_t managerCountBefore{},managerCountAfter{};
    bool managerReadyBefore{},managerReadyAfter{},originalForwarded{};
    std::span<const std::byte> priorEntry{};
    std::uint64_t entryIndex{UINT64_MAX};
};
struct Observation final {
    Ticket ticket{};Source source{};std::uint64_t sequence{},managerIndex{};
    std::uint32_t managerId{};
};
enum class Result:std::uint8_t {accepted,identity,body,unchanged,managerUnavailable,managerEntry};
[[nodiscard]] inline bool presentation_matches(const Ticket& t,std::span<const std::byte> entry,std::int32_t mode=1) noexcept {
    if(t.presentation==Presentation::activityObjective)
        return field<std::int32_t>(entry,0x70)==mode
            && field<std::uint32_t>(entry,0x58)==t.stringBank && field<std::uint32_t>(entry,0x5C)==t.title;
    // Native record+37=1 selects its own heading instead of the activity catalog.
    // Original1009740 must accept the first scoped activation reference: its
    // absent/not-ready path emits record+35=1 and child mode0, which is rejected.
    return t.presentation==Presentation::authoredProgress
        && field<std::uint8_t>(entry,0x28)==1
        && field<std::uint32_t>(entry,0x18)==t.stringBank && field<std::uint32_t>(entry,0x1C)==t.title
        && field<std::uint32_t>(entry,0x58)==t.stringBank && field<std::uint32_t>(entry,0x5C)==t.detail
        && field<std::uint32_t>(entry,0x60)==t.stringBank && field<std::uint32_t>(entry,0x64)==t.progressLabel
        && field<std::int32_t>(entry,0x70)==mode
        && field<std::uint8_t>(entry,0x74)==static_cast<std::uint8_t>(t.progressFormat);
}
[[nodiscard]] inline Result qualify(const Ticket& ticket,const Capture& c,Observation& out) noexcept {
    out={};
    if(!valid(ticket) || c.ticket!=ticket || !c.originalForwarded || c.producerRva!=kProducerRva
        || !c.sequence || c.source.member==UINT32_MAX || c.source.offset<0)return Result::identity;
    if(c.prior.size()!=wire::kDecodedBytes || !wire::matches_fields(c.incoming,ticket.request)
        || c.applied.size()!=c.incoming.size() || !std::equal(c.applied.begin(),c.applied.end(),c.incoming.begin()))return Result::body;
    // Qualify every decoded field, then require the complete incoming image,
    // including native-owned padding, to have been copied by the original.
    if(wire::matches_fields(c.prior,ticket.request))return Result::unchanged;
    if(!c.managerReadyBefore || !c.managerReadyAfter)return Result::managerUnavailable;
    if(ticket.request.clear) {
        auto active=ticket.request;active.clear=false;
        if(!wire::matches_fields(c.prior,active))return Result::body;
        if(!c.managerCountBefore || c.managerCountBefore>16 || c.managerCountAfter!=c.managerCountBefore
            || c.entryIndex>=c.managerCountBefore || c.priorEntry.size()!=0x148 || c.entry.size()!=0x148
            || field<std::uint8_t>(c.entry,0)!=2
            || field<std::uint32_t>(c.entry,4)!=wire::manager_id(ticket.request.event,ticket.request.variant)
            || field<std::uint32_t>(c.entry,0x10)!=ticket.request.registry
            || field<std::int16_t>(c.entry,0x124)!=ticket.activity
            || field<std::uint64_t>(c.entry,0x110)!=1
            || !presentation_matches(ticket,c.priorEntry,ticket.incoming?0:1) || !presentation_matches(ticket,c.entry,3)
            || field<std::uint8_t>(c.priorEntry,0x2B)!=2 || field<std::uint8_t>(c.entry,0x2B)!=1)
            return Result::managerEntry;
        // Original137E2C0/137CC60 changes only lifecycle+2B and mode+70.
        for(std::size_t i=0;i<c.entry.size();++i)
            if(i!=0x2B && (i<0x70 || i>=0x74) && c.entry[i]!=c.priorEntry[i])return Result::managerEntry;
        out={ticket,c.source,c.sequence,c.entryIndex,wire::manager_id(ticket.request.event,ticket.request.variant)};
        return Result::accepted;
    }
    if(c.managerCountBefore>=16 || c.managerCountAfter!=c.managerCountBefore+1 || c.entry.size()!=0x148
        || field<std::uint8_t>(c.entry,0)!=2
        || field<std::uint32_t>(c.entry,4)!=wire::manager_id(ticket.request.event)
        || field<std::uint32_t>(c.entry,0x10)!=ticket.request.registry
        || field<std::int16_t>(c.entry,0x124)!=ticket.activity
        || field<std::uint64_t>(c.entry,0x110)!=1 || !presentation_matches(ticket,c.entry,ticket.incoming?0:1)
        || field<std::uint32_t>(c.entry,0x20)!=ticket.stringBank || field<std::uint32_t>(c.entry,0x24)!=ticket.detail
        || !(field<std::uint8_t>(c.entry,0x140)&1) || field<std::uint8_t>(c.entry,0x141)!=1
        || !(field<std::uint8_t>(c.entry,0x2C)&4))return Result::managerEntry;
    out={ticket,c.source,c.sequence,c.managerCountBefore,wire::manager_id(ticket.request.event)};
    return Result::accepted;
}
}
