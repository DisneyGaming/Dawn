#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dawn::state::activity::coo {
// The binding must supply an authenticated owner and event. Publication time
// is deliberately not a clock origin. Duplicate events never restart a clock.
template<class Owner,std::size_t Events=32>
class EventTimeline final {
public:
    void reset() noexcept { *this={}; }
    bool bind(const Owner& owner) noexcept {
        if(!owner.valid() || owner_.valid()) { return false; }owner_=owner;return true;
    }
    bool mark(const Owner& owner,std::size_t event,std::uint64_t now) noexcept {
        if(!owner.valid() || owner!=owner_ || event>=Events || seen_[event]) { return false; }
        seen_[event]=true;times_[event]=now;return true;
    }
    bool seen(std::size_t event) const noexcept { return event<Events && seen_[event]; }
    bool elapsed(std::size_t event,std::uint64_t now,std::uint64_t delay) const noexcept {
        return seen(event) && now>=times_[event] && now-times_[event]>=delay;
    }
    Owner owner() const noexcept { return owner_; }
private:
    Owner owner_{};std::array<bool,Events> seen_{};std::array<std::uint64_t,Events> times_{};
};
}
