#pragma once
#include "coo/native_population_ledger.h"
#include "lifecycle_generation.h"
#include <span>

namespace sunrise::state::activity::native_population {
struct Lease final {
    ActivityInstanceKey activity{};
    coo::PopulationOwner source{};
    std::uint8_t bubble{};
    friend bool operator==(const Lease&,const Lease&)=default;
};
enum class Kind : std::uint8_t { admitted, died, retired };
struct Event final {Lease lease{};coo::PopulationActor actor{};std::uint32_t sourceHandle{UINT32_MAX};Kind kind{};};
// Pure bounded mailbox. It does not infer native retirement or run spawn policy.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const Lease& lease) noexcept {
        if(!lease.activity || !lease.source.valid() || lease.bubble>63
            || lease.source.activity!=lease.activity.sessionId || lease.source.incarnation!=lease.activity.incarnation.value
            || lease.source.source.type!=1) return false;
        for(std::size_t i=0;i<used_;++i) {
            if(leases_[i]==lease) return true;
            if(leases_[i].activity==lease.activity && leases_[i].source.source==lease.source.source) return false;
        }
        if(used_==leases_.size() || epoch_==UINT64_MAX) return false;
        leases_[used_++]=lease;++epoch_;return true;
    }
    void release(ActivityInstanceKey owner) noexcept {
        bool changed{};
        for(std::size_t i=0;i<used_;) {
            if(leases_[i].activity==owner) {leases_[i]=leases_[--used_];changed=true;} else ++i;
        }
        for(std::size_t i=0;i<queued_;) {
            if(events_[i].lease.activity==owner) erase(i);else ++i;
        }
        if(changed && epoch_!=UINT64_MAX) ++epoch_;
    }
    void unbind(const Lease& lease) noexcept {
        bool changed{};
        for(std::size_t i=0;i<used_;)
            if(leases_[i]==lease) {leases_[i]=leases_[--used_];changed=true;} else ++i;
        for(std::size_t i=0;i<queued_;)
            if(events_[i].lease==lease) erase(i);else ++i;
        if(changed && epoch_!=UINT64_MAX) ++epoch_;
    }
    [[nodiscard]] Lease lookup(std::uint32_t definition,std::uint32_t registry,
        std::uint16_t slot,std::uint32_t generation) const noexcept {
        const Lease* found{};
        for(std::size_t i=0;i<used_;++i) {
            const auto& lease=leases_[i];
            if(lease.source.source.definition==definition && lease.source.source.registry==registry
                && lease.source.source.slot==slot && lease.source.generation==generation) {
                if(found) return {};found=&lease;
            }
        }
        return found?*found:Lease{};
    }
    [[nodiscard]] bool submit(const Event& event,std::uint64_t epoch) noexcept {
        if(epoch!=epoch_ || (event.kind!=Kind::admitted && event.kind!=Kind::died && event.kind!=Kind::retired)
            || !event.actor.valid() || event.sourceHandle==UINT32_MAX
            || event.actor.owner!=event.lease.source) return false;
        bool bound{};for(std::size_t i=0;i<used_;++i) bound|=leases_[i]==event.lease;
        if(!bound) return false;
        if(queued_==events_.size()) {overflow_=true;return false;}
        events_[queued_++]=event;return true;
    }
    [[nodiscard]] std::size_t drain(ActivityInstanceKey owner,std::span<Event> output) noexcept {
        std::size_t count{};
        for(std::size_t i=0;i<queued_ && count<output.size();) {
            if(events_[i].lease.activity==owner) {output[count++]=events_[i];erase(i);}else ++i;
        }
        return count;
    }
    [[nodiscard]] std::uint64_t epoch() const noexcept {return used_?epoch_:0;}
    [[nodiscard]] bool pending(ActivityInstanceKey owner) const noexcept {
        if(!owner)return false;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].lease.activity==owner)return true;
        return false;
    }
    [[nodiscard]] bool overflow() const noexcept {return overflow_;}
    void observation_lost() noexcept {overflow_=true;}
private:
    void erase(std::size_t index) noexcept {for(std::size_t i=index+1;i<queued_;++i) events_[i-1]=events_[i];--queued_;}
    std::array<Lease,128> leases_{};std::size_t used_{};
    std::array<Event,256> events_{};std::size_t queued_{};
    std::uint64_t epoch_{1};bool overflow_{};
};
// Synchronized bridge. Native hooks copy and qualify records before calling it;
// the authoritative activity update drains them without invoking native code.
[[nodiscard]] bool bind(const Lease&) noexcept;
void unbind(const Lease&) noexcept;
void release(ActivityInstanceKey) noexcept;
[[nodiscard]] std::uint64_t epoch() noexcept;
// Scheduling hint only. The authoritative owner still drains and validates each
// observation; inspecting this hint never consumes or acknowledges a receipt.
[[nodiscard]] bool pending(ActivityInstanceKey) noexcept;
[[nodiscard]] Lease lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept;
[[nodiscard]] bool submit(const Event&,std::uint64_t epoch) noexcept;
void observation_lost() noexcept;
[[nodiscard]] std::size_t drain(ActivityInstanceKey,std::span<Event>,bool& overflow) noexcept;
}
