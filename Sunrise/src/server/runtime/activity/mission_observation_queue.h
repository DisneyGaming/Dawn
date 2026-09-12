#pragma once
#include <array>
#include <cstddef>
namespace sunrise::server::runtime::activity {
// Caller holds its mission mutex. Observers copy authenticated evidence here;
// the server snapshot is the only consumer. Overflow stops publication until
// a new owner is selected, rather than silently dropping a terminal receipt.
template<class Event,std::size_t Capacity=256> class MissionObservationQueue final {
public:
    bool push(const Event& event) noexcept {
        if(failed_ || count_==Capacity) {failed_=true;return false;}
        events_[count_++]=event;return true;
    }
    template<class Apply> bool drain(Apply apply) noexcept {
        if(failed_)return false;
        for(std::size_t i=0;i<count_;++i)apply(events_[i]);
        count_=0;return true;
    }
    void fail() noexcept {failed_=true;}
    void reset() noexcept {count_=0;failed_=false;}
private:
    std::array<Event,Capacity> events_{};std::size_t count_{};bool failed_{};
};
}
