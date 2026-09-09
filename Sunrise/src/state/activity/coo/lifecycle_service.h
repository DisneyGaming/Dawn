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
    Generation owner{};bool complete{};
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
    void reset() noexcept { owner_={};complete_=false; }
    Generation owner() const noexcept { return owner_; }
    bool complete(Generation owner) noexcept {
        if(!owner.valid() || owner!=owner_ || complete_) { return false; }
        complete_=true;return true;
    }
    CompletionPublication publication() const noexcept { return {owner_,complete_}; }
    bool finished() const noexcept { return complete_; }
    std::uint8_t activity_state() const noexcept { return complete_?6U:0U; }
private:
    Generation owner_{};std::uint32_t high_{};bool complete_{};
};
}
