#pragma once
#include "public_event_participant_bridge.h"
#include "public_event_engagement_runtime.h"
#include "adventure_native_bridge.h"
#include <optional>

namespace dawn::server::runtime::activity::public_event {
namespace incoming_cues=adventure::native_bridge;
struct IncomingDefinition final {
    participant_feedback::Ticket participant{};
    engagement_feedback::Ticket empty{};
    adventure::cue_feedback::Ticket cue{};
};
struct IncomingFrame final {
    participant_feedback::wire::Request participant{};
    engagement_feedback::wire::Request engagement{};
    adventure::cue_feedback::wire::Request cue{};
    bool publishParticipant{},publishEngagement{},publishCue{};
    bool readyForWorld{},joinApplied{},joined{},stale{},failed{};
};
// World incoming and participation have independent lifetimes. Bind/qualify the
// type71 local owner and empty type70 before the incoming directive can reach
// original1008B40. It snapshots type71+199 only when creating the manager entry.
// A later authorized join advances only type70, so original100A3A0 changes the
// same incoming entry to active without another insertion or invented variant.
class IncomingRuntime final {
public:
    [[nodiscard]] bool begin(const IncomingDefinition& d) noexcept {
        if(bound_ || !participant_feedback::valid(d.participant)
            || !engagement_feedback::valid(d.empty) || !adventure::cue_feedback::valid(d.cue)
            || d.empty.request.collection!=engagement_feedback::wire::Collection::none
            || d.empty.request.generation<=0 || d.empty.request.generation==INT16_MAX
            || !d.cue.incoming || d.cue.request.clear)return false;
        const auto& p=d.participant;const auto& e=d.empty;const auto& c=d.cue;
        if(p.owner!=e.owner || p.owner!=c.owner || p.boot!=e.boot || p.boot!=c.boot
            || p.definitionRevision!=e.definitionRevision || p.definitionRevision!=c.definitionRevision
            || p.selectionRevision!=e.selectionRevision || p.selectionRevision!=c.selectionRevision
            || p.event!=e.event || p.request.scope!=e.request.scope || p.request.scope!=c.request.scope
            || c.request.publicEvent!=participant_feedback::cue::Reference{p.request.registry,71,p.request.slot}
            || c.request.readiness!=participant_feedback::cue::Reference{e.request.registry,70,e.request.slot})return false;
        definition_=d;bound_=true;return true;
    }
    [[nodiscard]] IncomingFrame update(const EngagementContext& current) noexcept {
        eligible_=false;
        if(!bound_ || stale_ || failed_)return frame();
        const auto& p=definition_.participant;
        if(current.owner!=p.owner || current.boot!=p.boot || current.definitionRevision!=p.definitionRevision
            || current.selectionRevision!=p.selectionRevision || current.event!=p.event
            || current.activity!=definition_.cue.activity){stale_=true;return frame();}
        eligible_=current.arrived && current.admitted && current.bubble==p.request.scope;
        if(!eligible_)return frame();
        if(!published_) {
            if(!participant_bridge::bind(p) || !engagement_bridge::bind(definition_.empty)){failed_=true;return frame();}
            published_=true;
        }
        std::array<participant_bridge::Event,1> participants{};
        for(std::size_t i=0,n=participant_bridge::drain(p,participants);i<n;++i) {
            if(participants[i].binding.ticket!=p || participants[i].observation.ticket!=p){failed_=true;return frame();}
            participantApplied_=true;
        }
        std::array<engagement_bridge::Event,1> empties{};
        for(std::size_t i=0,n=engagement_bridge::drain(definition_.empty,empties);i<n;++i) {
            if(empties[i].observation.ticket!=definition_.empty){failed_=true;return frame();}
            emptyApplied_=true;
        }
        if(participantApplied_ && emptyApplied_ && !cuePublished_) {
            if(!incoming_cues::bind(definition_.cue)){failed_=true;return frame();}
            cuePublished_=true;
        }
        std::array<incoming_cues::Event,1> cues{};
        for(std::size_t i=0,n=incoming_cues::drain(definition_.cue,cues);i<n;++i) {
            if(cues[i].observation.ticket!=definition_.cue){failed_=true;return frame();}
            cueReceipt_=cues[i];incomingApplied_=true;
        }
        if(incomingApplied_ && joinRequested_ && !joinPublished_) {
            joinedTicket_=definition_.empty;
            joinedTicket_.request.collection=engagement_feedback::wire::Collection::activePlayers;
            ++joinedTicket_.request.generation;
            if(!engagement_bridge::join(definition_.empty,joinedTicket_)
                || !joinedRuntime_.begin(joinedTicket_,definition_.cue.activity)){failed_=true;return frame();}
            joinPublished_=true;
        }
        if(joinPublished_) {
            const auto joined=joinedRuntime_.update(current);
            if(joined.failed || joined.stale){failed_=true;return frame();}
            joinApplied_=joined.applied;joined_=joined.readyForCue;
        }
        return frame();
    }
    // Caller must supply a qualified participation event. For the current test
    // candidate only, the explicitly authorized rally shortcut is that signal.
    [[nodiscard]] bool request_join(const EngagementContext& current) noexcept {
        const auto& p=definition_.participant;
        if(!bound_ || stale_ || failed_ || current.owner!=p.owner || current.boot!=p.boot
            || current.definitionRevision!=p.definitionRevision || current.selectionRevision!=p.selectionRevision
            || current.event!=p.event || current.activity!=definition_.cue.activity
            || !current.arrived || !current.admitted || current.bubble!=p.request.scope)return false;
        joinRequested_=true;return true;
    }
    [[nodiscard]] bool observe(const engagement_feedback::Ticket& ticket,std::uint32_t bubble,
        std::uint32_t schema,const engagement_sense::Output& sense) noexcept {
        return joinPublished_ && !stale_ && !failed_ && joinedRuntime_.observe(ticket,bubble,schema,sense);
    }
    [[nodiscard]] IncomingFrame frame() const noexcept {
        const bool eligible=bound_ && eligible_ && !stale_ && !failed_;
        return {definition_.participant.request,joinPublished_?joinedTicket_.request:definition_.empty.request,
            definition_.cue.request,eligible && published_,eligible && published_,eligible && cuePublished_,
            eligible && incomingApplied_,joinApplied_,eligible && joined_,stale_,failed_};
    }
    [[nodiscard]] const IncomingDefinition& definition() const noexcept {return definition_;}
    [[nodiscard]] const std::optional<incoming_cues::Event>& cue_receipt() const noexcept {return cueReceipt_;}
    [[nodiscard]] const engagement_feedback::Ticket& engagement_ticket() const noexcept {
        return joinPublished_?joinedTicket_:definition_.empty;
    }
private:
    IncomingDefinition definition_{};
    std::optional<incoming_cues::Event> cueReceipt_{};
    engagement_feedback::Ticket joinedTicket_{};EngagementRuntime joinedRuntime_{};
    bool bound_{},eligible_{},published_{},participantApplied_{},emptyApplied_{},cuePublished_{},incomingApplied_{};
    bool joinRequested_{},joinPublished_{},joinApplied_{},joined_{},stale_{},failed_{};
};
}
