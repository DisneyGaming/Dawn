#pragma once
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../middleware/bap/activity_message/native/adventure_dialogue_authority.h"
#include <algorithm>
namespace dawn::server::runtime::activity::adventure::dialogue_feedback {
namespace wire=middleware::bap::activity_message::native::dialogue;
using Owner=state::activity::ActivityInstanceKey;
struct Authored final {
    std::uint32_t definition{},bank{},selector{};
    std::uint32_t durationMs{};
    std::uint8_t bankRows{},row{};
    friend bool operator==(const Authored&,const Authored&)=default;
};
[[nodiscard]] constexpr bool valid(const Authored& a) noexcept {
    const auto key=[](std::uint32_t x) {return x && x!=UINT32_MAX && x!=wire::kAbsent;};
    return key(a.definition) && key(a.bank) && key(a.selector) && a.durationMs && a.bankRows && a.bankRows<=128 && a.row<a.bankRows;
}
struct Ticket final {
    Owner owner{};std::uint64_t boot{},definitionRevision{},selectionRevision{};
    std::int16_t activity{-1};Authored authored{};wire::Request request{};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return t.owner && t.boot && t.definitionRevision && t.selectionRevision && t.activity>=0
        && valid(t.authored) && wire::valid(t.request) && t.request.bankRows==t.authored.bankRows
        && t.request.activeRow==t.authored.row;
}
struct Source final {std::uint32_t member{UINT32_MAX};std::int64_t offset{-1};friend bool operator==(const Source&,const Source&)=default;};
struct Capture final {
    Ticket ticket{};Source source{};std::uint64_t sequence{};
    std::span<const std::byte> before,after;
    std::uint32_t processedBefore{},processedAfter{};
    bool originalForwarded{},identityStable{};
};
struct Observation final {Ticket ticket{};Source source{};std::uint64_t sequence{};std::uint32_t before{},after{};};
enum class Result:std::uint8_t {accepted,identity,body,unchanged,notSubmitted};
[[nodiscard]] inline Result qualify(const Ticket& ticket,const Capture& c,Observation& out) noexcept {
    out={};
    if(!valid(ticket) || c.ticket!=ticket || !c.originalForwarded || !c.identityStable || !c.sequence
        || c.source.member==UINT32_MAX || c.source.offset<0)return Result::identity;
    if(!wire::matches_fields(c.before,ticket.request) || c.after.size()!=c.before.size()
        || !std::equal(c.before.begin(),c.before.end(),c.after.begin()))return Result::body;
    const auto generation=ticket.request.generations[ticket.authored.row];
    if(c.processedBefore==generation)return Result::unchanged;
    // Original100A180 stores a mode2 generation only after10097D0 returns.
    // This is submission, not the later audio-arbitration/playback completion.
    if(c.processedAfter!=generation)return Result::notSubmitted;
    out={ticket,c.source,c.sequence,c.processedBefore,c.processedAfter};return Result::accepted;
}
}
