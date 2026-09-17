#pragma once
#include "../../../middleware/bap/activity_message/native/adventure_cue_authority.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <algorithm>
#include <limits>
#include <optional>

namespace dawn::server::runtime::activity::cue_presentation {
namespace wire=middleware::bap::activity_message::native::cue;
using Owner=state::activity::ActivityInstanceKey;
enum class TimerOperation : std::uint8_t { absent,start,retain,pause,resume };
/** Largest signed value representable by the native variant and progress lanes. */
inline constexpr std::int64_t kMaximumDynamicValue=(std::numeric_limits<std::int32_t>::max)();
// Authored action contract. No native addresses, population counts or arbitrary
// per-command event/variant/time configuration arrive through Command. A present
// maximumVariant enables server-only variant changes through PresentationUpdate;
// its inclusive bound is also applied to the initial presentation. A present
// progressTarget enables server-only active-ring progress changes; it is the
// exact native target used by each update. Both capabilities are disabled when
// absent, preserving legacy action behavior.
struct Action final {
    std::uint32_t id{};wire::Request presentation{};TimerOperation timer{};
    std::uint64_t durationTicks{};
    std::uint32_t requiredEvidence{};
    /** Optional inclusive upper bound for server-only variant updates. */
    std::optional<std::int64_t> maximumVariant{};
    /** Optional fixed native progress target for server-only active-ring updates. */
    std::optional<std::int64_t> progressTarget{};
};
struct Command final {
    Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t action{};
    // Already qualified server observations, not player-supplied evidence.
    std::uint32_t evidence{};
    // Same native scenario-clock tick domain as 4ECC50/3CAF00. No wall-clock epoch.
    std::uint64_t clockTicks{};
};
/** Server-only update of fields explicitly enabled by the current Action. */
struct PresentationUpdate final {
    Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t expectedAction{};
    std::optional<std::int32_t> variant{};
    std::optional<wire::Progress> progress{};
};
enum class Result : std::uint8_t {
    accepted,unchanged,stale,duplicate,unsupported,missingEvidence,invalidClock,exhausted,invalidUpdate
};
class Service final {
public:
    // Trusted NativeActivityDefinition path. Final snapshot serialization must
    // additionally validate project() against the actual admitted native roster.
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Action> actions) noexcept {
        if(owner_ || !owner || !boot || actions.empty() || actions.size()>actions_.size())return false;
        for(std::size_t i=0;i<actions.size();++i) {
            const auto& a=actions[i];
            if(!a.id || a.presentation.hasTimer || !wire::valid(a.presentation)
                || a.timer>TimerOperation::resume
                || (a.timer==TimerOperation::start && (!a.durationTicks || a.durationTicks>wire::Timer{}.maximum))
                || (a.timer!=TimerOperation::start && a.durationTicks)
                || (a.maximumVariant && (*a.maximumVariant<0 || *a.maximumVariant>kMaximumDynamicValue))
                || (a.progressTarget && (*a.progressTarget<=0 || *a.progressTarget>kMaximumDynamicValue))
                || (a.maximumVariant && static_cast<std::int64_t>(a.presentation.variant)>*a.maximumVariant)
                || (a.progressTarget && a.presentation.hasProgress
                    && a.presentation.progress.target!=*a.progressTarget))return false;
            if(i && (a.presentation.registry!=actions[0].presentation.registry
                || a.presentation.slot!=actions[0].presentation.slot
                || a.presentation.scope!=actions[0].presentation.scope))return false;
            for(std::size_t j=0;j<i;++j)if(actions[j].id==a.id)return false;
        }
        std::copy(actions.begin(),actions.end(),actions_.begin());count_=actions.size();
        owner_=owner;boot_=boot;revision_=1;return true;
    }
    template<class Roster> [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Action> actions,const Roster& roster) noexcept {
        for(const auto& a:actions) {wire::Batch b;b.count=1;b.entries[0]=a.presentation;if(!wire::valid(b,roster))return false;}
        return begin(owner,boot,actions);
    }
    [[nodiscard]] Result request(const Command& c) noexcept {
        if(!owner_ || c.owner!=owner_ || c.boot!=boot_ || c.expectedRevision!=revision_)return Result::stale;
        if(!c.request || c.request<=lastRequest_)return Result::duplicate;
        const Action* a{};for(std::size_t i=0;i<count_;++i)if(actions_[i].id==c.action)a=&actions_[i];
        if(!a)return Result::unsupported;
        if((c.evidence&a->requiredEvidence)!=a->requiredEvidence)return Result::missingEvidence;
        if(revision_==UINT64_MAX)return Result::exhausted;
        auto next=a->presentation;
        if(a->timer==TimerOperation::start && started_) {
            if(currentAction_!=a->id)return Result::unsupported;
            lastRequest_=c.request;++revision_;return Result::unchanged; // Never reset an active run.
        }
        if(a->timer==TimerOperation::start) {
            if(c.clockTicks==UINT64_MAX)return Result::invalidClock;
            next.hasTimer=true;next.timer={};next.timer.advancing=true;
            next.timer.remaining=a->durationTicks;next.timer.anchor=c.clockTicks;
        } else if(a->timer!=TimerOperation::absent) {
            if(!started_ || !published_ || !current_.hasTimer)return Result::unsupported;
            next.hasTimer=true;next.timer=current_.timer;
            if(a->timer==TimerOperation::pause || a->timer==TimerOperation::resume) {
                if(c.clockTicks==UINT64_MAX || c.clockTicks<next.timer.anchor)return Result::invalidClock;
                if(next.timer.advancing) {
                    // This service owns normal-rate run timers only.
                    const auto delta=std::min(c.clockTicks-next.timer.anchor,next.timer.maximum-next.timer.elapsed);
                    next.timer.elapsed+=delta;next.timer.remaining-=std::min(delta,next.timer.remaining);
                }
                next.timer.anchor=c.clockTicks;next.timer.advancing=a->timer==TimerOperation::resume;
            }
        }
        if(!wire::valid(next))return Result::unsupported;
        const bool unchanged=published_ && next==current_;
        current_=next;currentAction_=a->id;published_=true;
        if(a->timer==TimerOperation::start)started_=true;
        lastRequest_=c.request;++revision_;return unchanged?Result::unchanged:Result::accepted;
    }
    [[nodiscard]] Result update(const PresentationUpdate& u) noexcept {
        if(!owner_ || u.owner!=owner_ || u.boot!=boot_ || u.expectedRevision!=revision_)return Result::stale;
        if(!u.request || u.request<=lastRequest_)return Result::duplicate;
        if(!published_ || u.expectedAction!=currentAction_)return Result::stale;
        const Action* a{};for(std::size_t i=0;i<count_;++i)if(actions_[i].id==currentAction_)a=&actions_[i];
        if(!a || (!u.variant && !u.progress))return Result::unsupported;
        if(u.variant && !a->maximumVariant)return Result::unsupported;
        if(u.progress && (!a->progressTarget || current_.clear))return Result::unsupported;
        if(u.variant && (*u.variant<0 || static_cast<std::int64_t>(*u.variant)>*a->maximumVariant))
            return Result::invalidUpdate;
        if(u.progress) {
            const auto& p=*u.progress;
            if(p.target<=0 || p.current<0 || p.current>p.target
                || static_cast<std::int64_t>(p.target)>kMaximumDynamicValue
                || p.target!=*a->progressTarget)return Result::invalidUpdate;
        }
        if(revision_==UINT64_MAX)return Result::exhausted;
        auto next=current_;
        if(u.variant)next.variant=*u.variant;
        if(u.progress) {next.hasProgress=true;next.progress=*u.progress;}
        if(!wire::valid(next))return Result::invalidUpdate;
        const bool unchanged=next==current_;
        current_=next;lastRequest_=u.request;++revision_;
        return unchanged?Result::unchanged:Result::accepted;
    }
    [[nodiscard]] wire::Batch project() const noexcept {wire::Batch b;if(published_){b.count=1;b.entries[0]=current_;}return b;}
    [[nodiscard]] std::uint64_t revision() const noexcept {return revision_;}
    [[nodiscard]] std::uint64_t last_request() const noexcept {return lastRequest_;}
    [[nodiscard]] std::uint32_t current_action() const noexcept {return currentAction_;}
    [[nodiscard]] const wire::Request* presentation() const noexcept {return published_?&current_:nullptr;}
    [[nodiscard]] Owner owner() const noexcept {return owner_;}
    [[nodiscard]] std::uint64_t boot() const noexcept {return boot_;}
private:
    Owner owner_{};std::uint64_t boot_{},revision_{},lastRequest_{};std::array<Action,8> actions_{};
    std::size_t count_{};wire::Request current_{};std::uint32_t currentAction_{};bool published_{},started_{};
};
}
