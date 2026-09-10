#pragma once
#include "native_capture_feedback.h"
#include <array>
#include <atomic>
#include <mutex>

namespace sunrise::server::runtime::activity::capture_bridge {
namespace feedback=capture_feedback;
struct Event final {feedback::Observation observation{};bool ready{},completed{};};
class Mailbox final {
public:
    [[nodiscard]] bool bind(feedback::Ticket ticket) noexcept {
        if(!feedback::valid(ticket))return false;
        for(std::size_t i=0;i<count_;++i) {
            if(feedback::same(rows_[i].ticket,ticket))return true;
            if(rows_[i].ticket.source==ticket.source)return false;
        }
        if(count_==rows_.size())return false;
        // A retired slot may have completed an earlier owner. Reset its entire
        // observation state before admitting this exact new ticket.
        rows_[count_]={};rows_[count_++].ticket=ticket;return true;
    }
    [[nodiscard]] feedback::Ticket lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot) const noexcept {
        const feedback::Ticket* found{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& row=rows_[i];if(row.completed || row.ticket.controllerDefinition!=definition
                || row.ticket.source.registry!=registry || row.ticket.source.slot!=slot)continue;
            if(found)return {};found=&row.ticket;
        }
        return found?*found:feedback::Ticket{};
    }
    [[nodiscard]] bool interested(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)
            if(!rows_[i].completed && rows_[i].ticket.controllerDefinition==definition)return true;
        return false;
    }
    [[nodiscard]] bool submit(const feedback::Capture& capture) noexcept {
        for(std::size_t i=0;i<count_;++i) {
            auto& row=rows_[i];if(!feedback::same(row.ticket,capture.ticket) || row.completed
                || capture.sequence<=row.sequence)continue;
            feedback::Observation value{};if(!feedback::qualify(row.ticket,capture,value))return false;
            if(row.ready && (value.sourceHandle!=row.first.sourceHandle || value.entityHandle!=row.first.entityHandle
                || value.controllerHandle!=row.first.controllerHandle || value.clockContext!=row.first.clockContext))return false;
            row.sequence=capture.sequence;
            if(row.ready && !value.completed)return false;
            if(queued_==events_.size()){overflow_=true;return false;}
            events_[queued_++]={value,!row.ready,value.completed};
            if(!row.ready)row.first=value;
            row.ready=true;row.completed=value.completed;return true;
        }
        return false;
    }
    [[nodiscard]] std::size_t drain(activity_clock::Owner owner,std::span<Event> output) noexcept {
        std::size_t used{};
        for(std::size_t i=0;i<queued_ && used<output.size();)
            if(events_[i].observation.ticket.domain.owner==owner){output[used++]=events_[i];erase(i);}else ++i;
        return used;
    }
    void release(activity_clock::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;) {
            if(rows_[i].ticket.domain.owner!=owner){++i;continue;}
            --count_;
            if(i!=count_)rows_[i]=rows_[count_];
            rows_[count_]={};
        }
        for(std::size_t i=0;i<queued_;)if(events_[i].observation.ticket.domain.owner==owner)erase(i);else ++i;
    }
    [[nodiscard]] bool overflow() const noexcept {return overflow_;}
    [[nodiscard]] std::size_t size() const noexcept {return count_;}
private:
    void erase(std::size_t at) noexcept {
        for(std::size_t i=at+1;i<queued_;++i)events_[i-1]=events_[i];
        events_[--queued_]={};
    }
    struct Row {feedback::Ticket ticket{};feedback::Observation first{};std::uint64_t sequence{};bool ready{},completed{};};
    std::array<Row,16> rows_{};std::array<Event,32> events_{};std::size_t count_{},queued_{};bool overflow_{};
};
inline std::mutex mutex;
inline Mailbox mailbox;
inline std::atomic_size_t activeCount{};
inline std::uint64_t nextArm{};
[[nodiscard]] inline bool bind(feedback::Ticket& ticket) noexcept {
    std::lock_guard lock(mutex);if(nextArm==UINT64_MAX)return false;
    ticket.armEpoch=++nextArm;const bool ok=mailbox.bind(ticket);activeCount.store(mailbox.size());return ok;
}
[[nodiscard]] inline feedback::Ticket lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot) noexcept {
    if(!activeCount.load())return {};
    std::lock_guard lock(mutex);return mailbox.lookup(definition,registry,slot);
}
[[nodiscard]] inline bool interested(std::uint32_t definition) noexcept {
    if(!activeCount.load())return false;
    std::lock_guard lock(mutex);return mailbox.interested(definition);
}
[[nodiscard]] inline bool submit(const feedback::Capture& capture) noexcept {
    std::lock_guard lock(mutex);return mailbox.submit(capture);
}
[[nodiscard]] inline std::size_t drain(activity_clock::Owner owner,std::span<Event> output,bool& overflow) noexcept {
    std::lock_guard lock(mutex);overflow=mailbox.overflow();return mailbox.drain(owner,output);
}
inline void release(activity_clock::Owner owner) noexcept {
    std::lock_guard lock(mutex);mailbox.release(owner);activeCount.store(mailbox.size());
}
}
