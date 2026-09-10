#pragma once
#include "public_event_engagement_feedback.h"

namespace sunrise::server::runtime::activity::public_event::engagement_bridge {
namespace feedback=engagement_feedback;
struct Binding final {feedback::Ticket ticket{};std::uint64_t epoch{};};
struct Event final {Binding binding{};feedback::Observation observation{};};
// Bind before middleware publication. A definition has one retained authority
// ticket until its world owner retires; publication is never a native receipt.
// The sole renewal is the consumed empty incoming state to active enrollment.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const feedback::Ticket& ticket) noexcept {
        if(!feedback::valid(ticket) || (ticket.request.collection!=feedback::wire::Collection::activePlayers && ticket.request.collection!=feedback::wire::Collection::none))return false;
        for(std::size_t i=0;i<count_;++i) {
            if(rows_[i].binding.ticket==ticket)return true;
            if(rows_[i].binding.ticket.definition==ticket.definition)return false;
        }
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        rows_[count_++]={{ticket,++epoch_},false};return true;
    }
    // Only a fully consumed empty incoming authority may enroll the active
    // players, in the same immutable world/event and with one newer generation.
    [[nodiscard]] static bool can_join(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        auto expected=active;
        expected.request.collection=feedback::wire::Collection::activePlayers;
        if(active.request.generation==INT16_MAX)return false;
        expected.request.generation=static_cast<std::int16_t>(active.request.generation+1);
        return feedback::valid(active) && feedback::valid(next) && next==expected
            && active.request.collection==feedback::wire::Collection::none;
    }
    [[nodiscard]] bool join(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        if(!can_join(active,next) || epoch_==UINT64_MAX)return false;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].binding.ticket==active)return false;
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket==active) {
            if(!rows_[i].submitted)return false;
            rows_[i]={{next,++epoch_},false};return true;
        }
        return false;
    }
    [[nodiscard]] Binding lookup(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)
            if(rows_[i].binding.ticket.definition==definition && !rows_[i].submitted)return rows_[i].binding;
        return {};
    }
    [[nodiscard]] bool submit(const Event& event) noexcept {
        const auto& observation=event.observation;
        if(!event.binding.epoch || !feedback::valid(observation.ticket)
            || observation.ticket!=event.binding.ticket || !observation.sequence
            || observation.source.member==UINT32_MAX || observation.source.offset<0
            || observation.source.offset>=0x2000000)return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& row=rows_[i];
            if(row.binding.epoch!=event.binding.epoch || row.binding.ticket!=event.binding.ticket)continue;
            if(row.submitted || queued_==events_.size())return false;
            events_[queued_++]=event;row.submitted=true;return true;
        }
        return false;
    }
    [[nodiscard]] std::size_t drain(const feedback::Ticket& ticket,std::span<Event> output) noexcept {
        if(!feedback::valid(ticket))return 0;
        std::size_t count{};
        for(std::size_t i=0;i<queued_ && count<output.size();)
            if(events_[i].binding.ticket==ticket){output[count++]=events_[i];erase(i);}else ++i;
        return count;
    }
    void release(feedback::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;)
            if(rows_[i].binding.ticket.owner==owner)rows_[i]=rows_[--count_];else ++i;
        for(std::size_t i=0;i<queued_;)
            if(events_[i].binding.ticket.owner==owner)erase(i);else ++i;
    }
private:
    void erase(std::size_t index) noexcept {
        for(std::size_t i=index+1;i<queued_;++i)events_[i-1]=events_[i];--queued_;
    }
    struct Row {Binding binding{};bool submitted{};};
    std::array<Row,16> rows_{};std::array<Event,16> events_{};
    std::size_t count_{},queued_{};std::uint64_t epoch_{};
};
[[nodiscard]] bool bind(const feedback::Ticket&) noexcept;
[[nodiscard]] bool join(const feedback::Ticket&,const feedback::Ticket&) noexcept;
[[nodiscard]] Binding lookup(std::uint32_t definition) noexcept;
[[nodiscard]] bool submit(const Event&) noexcept;
[[nodiscard]] std::size_t drain(const feedback::Ticket&,std::span<Event>) noexcept;
void release(feedback::Owner) noexcept;
}
