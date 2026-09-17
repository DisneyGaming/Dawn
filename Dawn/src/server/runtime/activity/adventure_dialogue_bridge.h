#pragma once
#include "adventure_dialogue_feedback.h"
namespace dawn::server::runtime::activity::adventure::dialogue_bridge {
namespace feedback=dialogue_feedback;
struct Binding final {feedback::Ticket ticket{};std::uint64_t epoch{};};
struct Event final {Binding binding{};feedback::Observation observation{};};
class Mailbox final {
public:
    [[nodiscard]] bool bind(const feedback::Ticket& t) noexcept {
        if(!feedback::valid(t))return false;
        for(std::size_t i=0;i<count_;++i) {
            if(rows_[i].binding.ticket==t)return true;
            if(rows_[i].binding.ticket.authored.definition==t.authored.definition)return false;
        }
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        rows_[count_++]={{t,++epoch_},false};return true;
    }
    [[nodiscard]] Binding lookup(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket.authored.definition==definition && !rows_[i].submitted)return rows_[i].binding;
        return {};
    }
    // One consumed native submission may advance an authored bank row. All
    // previous row generations remain unchanged, so deactivating the prior row
    // cannot replay it or reset the consumer's per-row generation history.
    [[nodiscard]] static bool can_advance(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        if(!feedback::valid(active) || !feedback::valid(next)
            || active.authored.definition!=next.authored.definition || active.authored.bank!=next.authored.bank
            || active.authored.bankRows!=next.authored.bankRows)return false;
        const auto row=next.authored.row;
        if(active.request.generations[row]>=0x7FFFFFFFU)return false;
        auto expected=active;expected.authored=next.authored;expected.request.activeRow=row;
        expected.request.clearInactiveTimes=true;
        ++expected.request.generations[row];
        return next==expected;
    }
    [[nodiscard]] bool advance(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        if(!can_advance(active,next) || epoch_==UINT64_MAX)return false;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].binding.ticket==active)return false;
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket==active) {
            if(!rows_[i].submitted)return false;
            rows_[i]={{next,++epoch_},false};return true;
        }
        return false;
    }
    [[nodiscard]] bool submit(const Event& e) noexcept {
        const auto& o=e.observation;const auto& t=e.binding.ticket;
        if(!e.binding.epoch || !feedback::valid(t) || o.ticket!=t || !o.sequence || o.source.member==UINT32_MAX
            || o.source.offset<0 || o.before==o.after || o.after!=t.request.generations[t.authored.row])return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& r=rows_[i];if(r.binding.epoch!=e.binding.epoch || r.binding.ticket!=t)continue;
            if(r.submitted || queued_==events_.size())return false;
            events_[queued_++]=e;r.submitted=true;return true;
        }
        return false;
    }
    void release(feedback::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;)if(rows_[i].binding.ticket.owner==owner)rows_[i]=rows_[--count_];else ++i;
        for(std::size_t i=0;i<queued_;)if(events_[i].binding.ticket.owner==owner)erase(i);else ++i;
    }
    [[nodiscard]] std::size_t drain(const feedback::Ticket& ticket,std::span<Event> out) noexcept {
        std::size_t n{};for(std::size_t i=0;i<queued_ && n<out.size();)if(events_[i].binding.ticket==ticket){out[n++]=events_[i];erase(i);}else ++i;return n;
    }
private:
    void erase(std::size_t i) noexcept {for(std::size_t j=i+1;j<queued_;++j)events_[j-1]=events_[j];--queued_;}
    struct Row {Binding binding{};bool submitted{};};std::array<Row,16> rows_{};std::array<Event,16> events_{};
    std::size_t count_{},queued_{};std::uint64_t epoch_{};
};
[[nodiscard]] bool bind(const feedback::Ticket&) noexcept;
[[nodiscard]] bool advance(const feedback::Ticket&,const feedback::Ticket&) noexcept;
[[nodiscard]] Binding lookup(std::uint32_t definition) noexcept;
[[nodiscard]] bool submit(const Event&) noexcept;
[[nodiscard]] std::size_t drain(const feedback::Ticket&,std::span<Event>) noexcept;
void release(feedback::Owner) noexcept;
}
