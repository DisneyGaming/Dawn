#pragma once
#include "event_timeline.h"
#include <span>
namespace dawn::state::activity::coo {
enum class SceneSignal : std::uint8_t {
    greetingSubmitted, approached, animationReady, prerollFinished,
    conversationStarted, conversationFinished, sceneFinished
};
constexpr std::uint32_t signal_bit(SceneSignal value) noexcept { return 1U<<static_cast<unsigned>(value); }
struct AuthoredSceneEvent final { std::uint32_t id{},requiresSignals{}; };
// Native casting is requested by preload(). Signals can arrive before a graph
// waits for them. Conversation completion and the parent idle scene are separate.
template<class Owner,std::size_t MaxEvents=8>
class SceneOrchestration final {
public:
    bool preload(std::uint64_t run,std::uint32_t generation,std::span<const AuthoredSceneEvent> events) noexcept {
        if(!run || !generation || events.size()>MaxEvents || run_) { return false; }
        for(std::size_t i=0;i<events.size();++i) {
            if(!events[i].id || !events[i].requiresSignals || (events[i].requiresSignals&~127U)) { return false; }
            for(std::size_t j=0;j<i;++j) { if(events[j].id==events[i].id) { return false; } }
        }
        run_=run;generation_=generation;count_=events.size();
        for(std::size_t i=0;i<count_;++i) { policy_[i]=events[i]; }return true;
    }
    bool bind(const Owner& owner) noexcept {
        return owner.run==run_ && owner.generation==generation_ && timeline_.bind(owner);
    }
    bool signal(const Owner& owner,SceneSignal signal,std::uint64_t now) noexcept {
        if(signal==SceneSignal::conversationFinished && !seen(SceneSignal::conversationStarted)) { return false; }
        return timeline_.mark(owner,static_cast<unsigned>(signal),now);
    }
    bool seen(SceneSignal signal) const noexcept { return timeline_.seen(static_cast<unsigned>(signal)); }
    bool after(SceneSignal signal,std::uint64_t now,std::uint64_t delay) const noexcept {
        return timeline_.elapsed(static_cast<unsigned>(signal),now,delay);
    }
    void update(std::uint64_t now,std::uint32_t conversationDuration) noexcept {
        if(after(SceneSignal::conversationStarted,now,conversationDuration)) {
            static_cast<void>(signal(timeline_.owner(),SceneSignal::conversationFinished,now));
        }
    }
    std::size_t event_count() const noexcept {
        std::uint32_t mask{};for(unsigned i=0;i<7;++i) { if(timeline_.seen(i)) { mask|=1U<<i; } }
        std::size_t i{};for(;i<count_;++i) { if((policy_[i].requiresSignals&mask)!=policy_[i].requiresSignals) { break; } }return i;
    }
    std::uint32_t pending_signals() const noexcept {
        const auto i=event_count();if(i==count_) { return 0; }
        auto missing=policy_[i].requiresSignals;
        for(unsigned n=0;n<7;++n) { if(timeline_.seen(n)) { missing&=~(1U<<n); } }return missing;
    }
    std::uint32_t generation() const noexcept { return generation_; }
    Owner owner() const noexcept { return timeline_.owner(); }
private:
    EventTimeline<Owner,7> timeline_{};std::array<AuthoredSceneEvent,MaxEvents> policy_{};
    std::uint64_t run_{};std::uint32_t generation_{};std::size_t count_{};
};
}
