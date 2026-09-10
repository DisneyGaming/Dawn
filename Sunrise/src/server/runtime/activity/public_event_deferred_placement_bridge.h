#pragma once
#include "public_event_deferred_placement_feedback.h"
namespace sunrise::server::runtime::activity::public_event::deferred_bridge {
namespace feedback=deferred_placement;
struct Binding final {
    feedback::Ticket ticket{};std::uint64_t epoch{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
struct State final {
    Binding binding{};feedback::Observation creation{};
    std::uint64_t pointSequence{},pointWeak{};
    bool created{},ready{};
};
// One native authority generation per source definition and retained world.
// Creation and attached point-interface readiness are separate observations.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const feedback::Ticket& ticket) noexcept {
        if(!feedback::valid(ticket) || (ticket.pointComponent
            ? (!feedback::tag(ticket.pointComponent) || ticket.pointInterfaceOffset<=0 || ticket.pointInterfaceOffset>=0x2000000)
            : ticket.pointInterfaceOffset!=0))return false;
        for(std::size_t i=0;i<count_;++i){if(rows_[i].binding.ticket==ticket)return true;
            if(rows_[i].binding.ticket.definition==ticket.definition)return false;}
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        auto& row=rows_[count_++];row={};row.binding={ticket,++epoch_};return true;
    }
    [[nodiscard]] State lookup(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(rows_[i].binding.ticket.definition==definition)return rows_[i];return {};
    }
    [[nodiscard]] bool created(const Binding& binding,const feedback::Observation& observation) noexcept {
        if(!binding.epoch || observation.ticket!=binding.ticket || !observation.sequence
            || observation.child==UINT32_MAX || observation.source.member==UINT32_MAX
            || observation.source.componentLink==UINT32_MAX || observation.source.offset<0 || observation.source.offset>=0x2000000
            || (observation.source.member&0x1FFFU)!=(observation.source.componentLink&0x1FFFU)
            || static_cast<std::uint32_t>(observation.weakChild>>32)!=observation.child)return false;
        for(std::size_t i=0;i<count_;++i){auto& row=rows_[i];if(row.binding!=binding)continue;
            if(row.created)return false;row.creation=observation;row.created=true;return true;}return false;
    }
    [[nodiscard]] bool point_ready(const Binding& binding,const feedback::Observation& creation,
        std::uint64_t sequence,std::uint64_t weak) noexcept {
        if(!sequence || sequence<=creation.sequence || static_cast<std::uint32_t>(weak>>32)==UINT32_MAX)return false;
        for(std::size_t i=0;i<count_;++i){auto& row=rows_[i];if(row.binding!=binding)continue;
            if(!row.created || row.ready || creation.ticket!=row.creation.ticket || creation.source!=row.creation.source
                || creation.sequence!=row.creation.sequence || creation.child!=row.creation.child || creation.weakChild!=row.creation.weakChild)return false;
            row.pointSequence=sequence;row.pointWeak=weak;row.ready=true;return true;}return false;
    }
    void release(feedback::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;)if(rows_[i].binding.ticket.owner==owner)rows_[i]=rows_[--count_];else ++i;
    }
private:std::array<State,32> rows_{};std::size_t count_{};std::uint64_t epoch_{};
};
[[nodiscard]] bool bind(const feedback::Ticket&) noexcept;
[[nodiscard]] State lookup(std::uint32_t definition) noexcept;
[[nodiscard]] bool created(const Binding&,const feedback::Observation&) noexcept;
[[nodiscard]] bool point_ready(const Binding&,const feedback::Observation&,std::uint64_t sequence,std::uint64_t weak) noexcept;
void release(feedback::Owner) noexcept;
}
