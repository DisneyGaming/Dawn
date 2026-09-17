#pragma once
#include "adventure_cue_feedback.h"

namespace dawn::server::runtime::activity::adventure::native_bridge {
namespace feedback=cue_feedback;
struct Binding final {feedback::Ticket ticket{};std::uint64_t epoch{};};
struct Event final {Binding binding{};feedback::Observation observation{};};
// Bounded owner-scoped mailbox. Captures contain no native pointers. The server
// binds before publication and releases on owner retirement; old captures then
// fail even if a later activity reuses the same descriptor and source handle.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const feedback::Ticket& ticket) noexcept {
        if(!feedback::valid(ticket))return false;
        for(std::size_t i=0;i<count_;++i) {
            if(rows_[i].binding.ticket==ticket)return true;
            if(rows_[i].binding.ticket.definition==ticket.definition)return false;
        }
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        rows_[count_++]={{ticket,++epoch_},false};return true;
    }
    [[nodiscard]] Binding lookup(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)
            if(rows_[i].binding.ticket.definition==definition && !rows_[i].submitted)return rows_[i].binding;
        return {};
    }
    [[nodiscard]] bool submit(const Event& e) noexcept {
        const auto& o=e.observation;
        if(!e.binding.epoch || !feedback::valid(o.ticket) || o.ticket!=e.binding.ticket || !o.sequence
            || o.source.member==UINT32_MAX || o.source.offset<0 || o.managerIndex>=16
            || o.managerId!=feedback::wire::manager_id(o.ticket.request.event))return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& row=rows_[i];
            if(row.binding.epoch!=e.binding.epoch || row.binding.ticket!=e.binding.ticket)continue;
            if(row.submitted || queued_==events_.size())return false;
            events_[queued_++]=e;row.submitted=true;return true;
        }
        return false;
    }
    // Replace only a consumed, exactly matching activation ticket with its
    // deactivation. Other features sharing this world owner remain untouched.
    [[nodiscard]] bool clear(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        auto expected=active;expected.request.clear=true;
        if(active.request.clear || next!=expected || !feedback::valid(next) || epoch_==UINT64_MAX)return false;
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket==active) {
            if(!rows_[i].submitted)return false;
            rows_[i]={{next,++epoch_},false};
            for(std::size_t j=0;j<queued_;)if(events_[j].binding.ticket==active)erase(j);else ++j;
            return true;
        }
        return false;
    }
    // A graph may advance a consumed directive within the same authored table.
    // The next native ring makes the old ring absent through the normal codec.
    // Pending receipts, foreign lifetimes and same-ring replacements cannot
    // adopt an already existing native manager entry as a new activation.
    [[nodiscard]] static bool can_advance(const feedback::Ticket& active,const feedback::Ticket& next) noexcept {
        auto expected=active;
        expected.request=next.request;expected.detail=next.detail;expected.progressLabel=next.progressLabel;
        expected.progressFormat=next.progressFormat;expected.incoming=next.incoming;
        return feedback::valid(active) && feedback::valid(next) && expected==next
            && !active.request.clear && !next.request.clear
            && active.presentation==feedback::Presentation::authoredProgress
            && active.request.registry==next.request.registry && active.request.slot==next.request.slot
            && active.request.scope==next.request.scope && active.request.readiness==next.request.readiness
            && active.request.publicEvent==next.request.publicEvent && (!next.incoming || active.incoming)
            && active.request.event!=next.request.event && next.request.ring==(active.request.ring+1U)%3U;
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
    void release(feedback::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;)
            if(rows_[i].binding.ticket.owner==owner)rows_[i]=rows_[--count_];else ++i;
        for(std::size_t i=0;i<queued_;)
            if(events_[i].binding.ticket.owner==owner)erase(i);else ++i;
    }
    [[nodiscard]] bool pending(feedback::Owner owner) const noexcept {
        if(!owner)return false;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].binding.ticket.owner==owner)return true;
        return false;
    }
    [[nodiscard]] std::size_t drain(feedback::Owner owner,std::span<Event> output) noexcept {
        std::size_t n{};
        for(std::size_t i=0;i<queued_ && n<output.size();)
            if(events_[i].binding.ticket.owner==owner){output[n++]=events_[i];erase(i);}else ++i;
        return n;
    }
    // Feature consumers drain only their immutable ticket. Other cues may share
    // the world owner; their receipts retain their order in the same mailbox.
    [[nodiscard]] std::size_t drain(const feedback::Ticket& ticket,std::span<Event> output) noexcept {
        if(!feedback::valid(ticket))return 0;
        std::size_t n{};
        for(std::size_t i=0;i<queued_ && n<output.size();)
            if(events_[i].binding.ticket==ticket){output[n++]=events_[i];erase(i);}else ++i;
        return n;
    }
private:
    void erase(std::size_t i) noexcept {for(std::size_t j=i+1;j<queued_;++j)events_[j-1]=events_[j];--queued_;}
    struct Row {Binding binding{};bool submitted{};};
    std::array<Row,16> rows_{};std::array<Event,16> events_{};
    std::size_t count_{},queued_{};std::uint64_t epoch_{};
};
[[nodiscard]] bool bind(const feedback::Ticket&) noexcept;
[[nodiscard]] bool clear(const feedback::Ticket& active,const feedback::Ticket& next) noexcept;
[[nodiscard]] bool advance(const feedback::Ticket& active,const feedback::Ticket& next) noexcept;
[[nodiscard]] bool pending(feedback::Owner) noexcept;
void release(feedback::Owner) noexcept;
[[nodiscard]] Binding lookup(std::uint32_t definition) noexcept;
[[nodiscard]] bool submit(const Event&) noexcept;
[[nodiscard]] std::size_t drain(feedback::Owner,std::span<Event>) noexcept;
[[nodiscard]] std::size_t drain(const feedback::Ticket&,std::span<Event>) noexcept;
}
