#pragma once
#include <cstdint>
namespace sunrise::state::activity::coo {
struct Generation final {
    std::uint64_t run{};
    std::uint32_t value{};
    bool valid() const noexcept { return run!=0 && value!=0; }
    friend bool operator==(Generation,Generation)=default;
};
struct CompletionPublication final {
    Generation owner{};bool complete{};std::uint8_t state{6};
    bool valid() const noexcept { return owner.valid() && complete; }
};
// Reset retires the lease, never the high-water mark. A reused run therefore
// cannot accept receipts or device revisions from a previous attempt.
class LifecycleService final {
public:
    bool begin(std::uint64_t run,std::uint32_t reserve=2,std::uint32_t maximum=32766) noexcept {
        reset();
        if(!run || !reserve || reserve>maximum || high_>maximum-reserve) { return false; }
        owner_={run,high_+1};high_+=reserve;return true;
    }
    // Extend this owner's revision lease without changing its identity or completion.
    bool reserve_through(Generation owner,std::uint32_t value,std::uint32_t maximum=32766) noexcept {
        if(!owner.valid() || owner!=owner_ || value<owner_.value || value>maximum) { return false; }
        if(value>high_) { high_=value; }
        return true;
    }
    void reset() noexcept { owner_={};complete_=false;timed_=false;deadline_=0;state_=6; }
    Generation owner() const noexcept { return owner_; }
    bool complete(Generation owner) noexcept {
        if(!owner.valid() || owner!=owner_ || complete_) { return false; }
        complete_=true;return true;
    }
    bool complete_timed(Generation owner,std::uint64_t now) noexcept {
        if(!complete(owner)) { return false; }
        timed_=true;deadline_=now+30000;return true;
    }
    bool advance(std::uint64_t now) noexcept {
        if(!timed_ || !complete_ || state_>=8 || now<deadline_) { return false; }
        ++state_;deadline_+=30000;return true;
    }
    CompletionPublication publication() const noexcept { return {owner_,complete_,state_}; }
    bool finished() const noexcept { return complete_; }
    std::uint8_t activity_state() const noexcept { return complete_?state_:0U; }
private:
    Generation owner_{};std::uint32_t high_{};bool complete_{},timed_{};
    std::uint8_t state_{6};std::uint64_t deadline_{};
};
}
