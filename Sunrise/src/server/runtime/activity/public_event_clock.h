#pragma once
#include "native_activity_clock.h"
#include "../../../middleware/bap/activity_message/native/adventure_cue_authority.h"

namespace sunrise::server::runtime::activity::public_event {
// Event-local timing in the existing synchronized activity clock. A phase
// change may replace its deadline; an objective or region change must not.
class Clock final {
public:
    [[nodiscard]] bool observe(const activity_clock::Publication& clock) noexcept {
        if(!clock || clock.elapsedTicks==UINT64_MAX)return false;
        if(domain_ && (clock.domain!=domain_ || clock.elapsedTicks<now_))return false;
        if(!domain_){domain_=clock.domain;origin_=clock.elapsedTicks;}
        now_=clock.elapsedTicks;return true;
    }
    [[nodiscard]] bool after(std::uint64_t milliseconds) const noexcept {
        std::uint64_t ticks{};
        return domain_ && activity_clock::wire::from_milliseconds(milliseconds,ticks) && now_-origin_>=ticks;
    }
    [[nodiscard]] bool start_phase(std::uint32_t phase,std::uint64_t milliseconds) noexcept {
        std::uint64_t ticks{};
        if(!domain_ || !phase || !milliseconds || phase<phase_
            || !activity_clock::wire::from_milliseconds(milliseconds,ticks) || ticks>timer_.maximum)return false;
        if(phase==phase_)return true; // Retried publication cannot extend a deadline.
        timer_={};timer_.advancing=true;timer_.anchor=now_;timer_.remaining=ticks;phase_=phase;return true;
    }
    [[nodiscard]] bool project(middleware::bap::activity_message::native::cue::Request& cue) const noexcept {
        if(!phase_)return false;
        cue.hasTimer=true;cue.timer=timer_;return true;
    }
    [[nodiscard]] bool expired() const noexcept {return phase_ && now_-timer_.anchor>=timer_.remaining;}
    [[nodiscard]] std::uint64_t now() const noexcept {return domain_?now_:UINT64_MAX;}
    [[nodiscard]] explicit operator bool() const noexcept {return static_cast<bool>(domain_);}
private:
    activity_clock::Domain domain_{};
    middleware::bap::activity_message::native::cue::Timer timer_{};
    std::uint64_t origin_{},now_{};std::uint32_t phase_{};
};
}
