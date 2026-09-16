#pragma once
#include "coo/native_population_ledger.h"
#include "lifecycle_generation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::state::activity::native_population {
inline constexpr std::size_t kBindingCapacity=384;
inline constexpr std::size_t kCreationCapacity=1152;
inline constexpr std::size_t kProvisionalCapacity=1152;
inline constexpr std::size_t kEventCapacity=3456;
struct Lease final {
    ActivityInstanceKey activity{};
    coo::PopulationOwner source{};
    std::uint8_t bubble{};
    // Explicit policy from the owning activity, never inferred from an actor tag.
    bool discardStreamedReplicas{};
    friend bool operator==(const Lease&,const Lease&)=default;
};
enum class Kind : std::uint8_t { admitted, died, retired, sourceRecreated };
struct Event final {
    Lease lease{};coo::PopulationActor actor{};std::uint32_t sourceHandle{UINT32_MAX};Kind kind{};
    std::uint32_t previousSourceHandle{UINT32_MAX};
    // Admission-only metadata from an exact native member/source catalog join.
    // Unknown does not affect death/retirement identity or authorize lane refill.
    std::uint8_t memberCategory{UINT8_MAX};
};

// A receipt identifies one specific binding lifetime. The lease value alone is
// insufficient because release followed by an identical bind is a valid ABA.
struct Receipt final {
    Lease lease{};
    std::uint64_t nonce{};
    [[nodiscard]] explicit operator bool() const noexcept {return nonce!=0;}
    friend bool operator==(const Receipt&,const Receipt&)=default;
};

// A0D510 cannot identify the source until after its native original returns.
// This ticket keeps every renewal quiescent across that unidentified interval.
struct Creation final {
    std::uint64_t nonce{};
    [[nodiscard]] explicit operator bool() const noexcept {return nonce!=0;}
    friend bool operator==(const Creation&,const Creation&)=default;
};

class RenewResult final {
public:
    enum Value : std::uint8_t { rejected, renewed, busy };
    constexpr RenewResult(Value value) noexcept:value_(value) {}
    [[nodiscard]] constexpr operator bool() const noexcept {return value_==renewed;}
    friend constexpr bool operator==(RenewResult left,RenewResult right) noexcept {
        return left.value_==right.value_;
    }
    friend constexpr bool operator==(RenewResult left,Value right) noexcept {return left.value_==right;}
    friend constexpr bool operator==(Value left,RenewResult right) noexcept {return left==right.value_;}
private:
    Value value_;
};
enum class AdmitResult : std::uint8_t { admitted, busy, rejected };
enum class StageResult : std::uint8_t { staged, ended, rejected };

// Pure bounded mailbox. It does not infer native retirement or run spawn policy.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const Lease& lease) noexcept {
        if(!valid(lease)) return false;
        for(std::size_t i=0;i<used_;++i) {
            if(bindings_[i].lease==lease) return true;
            if(bindings_[i].lease.activity==lease.activity
                && bindings_[i].lease.source.source==lease.source.source) return false;
        }
        std::uint64_t nonce{};
        if(used_==bindings_.size() || !allocate(nonce)) return false;
        bindings_[used_++]={lease,nonce};return true;
    }

    [[nodiscard]] RenewResult renew(const Lease& prior,const Lease& next) noexcept {
        if(!valid(prior) || !valid(next) || next.activity!=prior.activity
            || next.bubble!=prior.bubble || next.discardStreamedReplicas!=prior.discardStreamedReplicas
            || next.source.activity!=prior.source.activity
            || next.source.run!=prior.source.run || next.source.incarnation!=prior.source.incarnation
            || next.source.source!=prior.source.source || prior.source.generation==0x7FFFFFFFU
            || next.source.generation!=prior.source.generation+1) return RenewResult::rejected;
        std::size_t binding=bindings_.size();
        for(std::size_t i=0;i<used_;++i) {
            if(bindings_[i].lease==next) return RenewResult::rejected;
            if(bindings_[i].lease==prior) binding=i;
        }
        if(binding==bindings_.size()) return RenewResult::rejected;
        if(inflight_!=0) return RenewResult::busy;
        const auto nonce=bindings_[binding].nonce;
        for(std::size_t i=0;i<queued_;++i)
            if(events_[i].nonce==nonce) return RenewResult::busy;
        for(std::size_t i=0;i<provisional_;++i)
            if(provisionals_[i].nonce==nonce) return RenewResult::busy;
        std::uint64_t nextNonce{};if(!allocate(nextNonce)) return RenewResult::rejected;
        bindings_[binding]={next,nextNonce};return RenewResult::renewed;
    }

    void unbind(const Lease& lease) noexcept {
        for(std::size_t i=0;i<used_;) {
            if(bindings_[i].lease==lease) bindings_[i]=bindings_[--used_];else ++i;
        }
        for(std::size_t i=0;i<queued_;)
            if(events_[i].event.lease==lease) erase_event(i);else ++i;
        for(std::size_t i=0;i<provisional_;)
            if(provisionals_[i].event.lease==lease) erase_provisional(i);else ++i;
    }

    void release(ActivityInstanceKey owner) noexcept {
        for(std::size_t i=0;i<used_;) {
            if(bindings_[i].lease.activity==owner) bindings_[i]=bindings_[--used_];
            else ++i;
        }
        for(std::size_t i=0;i<queued_;) {
            if(events_[i].event.lease.activity==owner) erase_event(i);else ++i;
        }
        for(std::size_t i=0;i<provisional_;) {
            if(provisionals_[i].event.lease.activity==owner) erase_provisional(i);else ++i;
        }
    }

    [[nodiscard]] Lease lookup(std::uint32_t definition,std::uint32_t registry,
        std::uint16_t slot,std::uint32_t generation) const noexcept {
        const auto found=capture(definition,registry,slot,generation);
        return found?found.lease:Lease{};
    }

    [[nodiscard]] Receipt capture(std::uint32_t definition,std::uint32_t registry,
        std::uint16_t slot,std::uint32_t generation) const noexcept {
        const Binding* found{};
        for(std::size_t i=0;i<used_;++i) {
            const auto& binding=bindings_[i];const auto& source=binding.lease.source;
            if(source.source.definition==definition && source.source.registry==registry
                && source.source.slot==slot && source.generation==generation) {
                if(found) return {};found=&binding;
            }
        }
        return found?Receipt{found->lease,found->nonce}:Receipt{};
    }

    [[nodiscard]] Receipt capture(const Lease& lease) const noexcept {
        for(std::size_t i=0;i<used_;++i)
            if(bindings_[i].lease==lease) return {lease,bindings_[i].nonce};
        return {};
    }

    [[nodiscard]] Creation begin_creation() noexcept {
        if(used_==0)return {};
        if(inflight_==creations_.size()) {overflow_=true;return {};}
        std::uint64_t nonce{};if(!allocate(nonce)) {overflow_=true;return {};}
        // floor is the newest binding that existed before the native original.
        creations_[inflight_++]={nonce,nextNonce_};return {nonce};
    }

    // Always consumes a valid creation ticket. A returned receipt means the
    // unidentified interval was atomically converted to a retained provisional.
    [[nodiscard]] StageResult stage(Creation creation,const Event& event,Receipt& output) noexcept {
        output={};
        std::size_t ticket=creations_.size();
        for(std::size_t i=0;i<inflight_;++i) if(creations_[i].nonce==creation.nonce) {ticket=i;break;}
        if(ticket==creations_.size()) return StageResult::rejected;
        const auto floor=creations_[ticket].floor;erase_creation(ticket);
        if(!valid_provisional(event) || !event.actor.birthNonce || event.actor.birthNonce!=creation.nonce)
            return StageResult::rejected;
        const auto receipt=capture(event.lease);
        // Reject a binding created after begin_creation(); this closes release /
        // identical-rebind ABA while the native original was running.
        if(!receipt || receipt.nonce>floor) return StageResult::ended;
        for(std::size_t i=0;i<provisional_;++i) {
            const auto& previous=provisionals_[i];
            if(previous.nonce==receipt.nonce && previous.event.actor.actor==event.actor.actor) {
                if(previous.event.sourceHandle!=event.sourceHandle
                    || previous.event.actor.birthNonce!=event.actor.birthNonce)return StageResult::rejected;
                output=receipt;return StageResult::staged;
            }
        }
        if(provisional_==provisionals_.size()) {overflow_=true;return StageResult::rejected;}
        provisionals_[provisional_++]={event,receipt.nonce};output=receipt;return StageResult::staged;
    }

    void cancel(Creation creation) noexcept {
        for(std::size_t i=0;i<inflight_;++i) if(creations_[i].nonce==creation.nonce) {
            erase_creation(i);return;
        }
    }

    [[nodiscard]] bool provisional(Receipt receipt,const Event& event) const noexcept {
        if(!current(receipt))return false;
        for(std::size_t i=0;i<provisional_;++i) {
            const auto& candidate=provisionals_[i];
            if(candidate.nonce==receipt.nonce && candidate.event.lease==event.lease
                && candidate.event.actor.actor==event.actor.actor
                && candidate.event.actor.birthNonce==event.actor.birthNonce
                && candidate.event.sourceHandle==event.sourceHandle) return true;
        }
        return false;
    }

    [[nodiscard]] AdmitResult admit(Receipt receipt,const Event& event,bool publish=true) noexcept {
        if(!valid_event(event) || event.kind!=Kind::admitted || !current(receipt)
            || receipt.lease!=event.lease) return AdmitResult::rejected;
        std::size_t staged=provisionals_.size();
        for(std::size_t i=0;i<provisional_;++i) {
            const auto& candidate=provisionals_[i];
            if(candidate.nonce==receipt.nonce && candidate.event.actor.actor==event.actor.actor
                && candidate.event.actor.birthNonce==event.actor.birthNonce
                && candidate.event.sourceHandle==event.sourceHandle) {staged=i;break;}
        }
        if(staged==provisionals_.size()) return AdmitResult::rejected;
        if(publish) {
            if(queued_==events_.size()) return AdmitResult::busy;
            events_[queued_++]={event,receipt.nonce};
        }
        erase_provisional(staged);return AdmitResult::admitted;
    }

    [[nodiscard]] bool submit(const Event& event,Receipt receipt) noexcept {
        if(!valid_event(event) || !current(receipt) || receipt.lease!=event.lease) return false;
        if(queued_==events_.size()) {overflow_=true;return false;}
        events_[queued_++]={event,receipt.nonce};return true;
    }

    // Compatibility for isolated callers that bind one lease and immediately
    // submit. Native hooks use binding receipts so unrelated renewal is isolated.
    [[nodiscard]] bool submit(const Event& event,std::uint64_t epochValue) noexcept {
        const auto receipt=capture(event.lease);
        return receipt && receipt.nonce==epochValue && submit(event,receipt);
    }

    [[nodiscard]] std::size_t drain(ActivityInstanceKey owner,std::span<Event> output) noexcept {
        std::size_t count{};
        for(std::size_t i=0;i<queued_ && count<output.size();) {
            if(events_[i].event.lease.activity==owner) {output[count++]=events_[i].event;erase_event(i);}else ++i;
        }
        return count;
    }

    [[nodiscard]] std::uint64_t epoch() const noexcept {return used_?nextNonce_:0;}

    [[nodiscard]] bool pending(ActivityInstanceKey owner) const noexcept {
        if(!owner)return false;
        // A pre-identity creation cannot yet be assigned to an owner.
        if(inflight_!=0)return true;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].event.lease.activity==owner)return true;
        for(std::size_t i=0;i<provisional_;++i)if(provisionals_[i].event.lease.activity==owner)return true;
        return false;
    }

    [[nodiscard]] bool pending_lease(const Lease& lease) const noexcept {
        if(!valid(lease))return false;
        if(inflight_!=0)return true;
        const auto receipt=capture(lease);if(!receipt)return false;
        for(std::size_t i=0;i<queued_;++i)if(events_[i].nonce==receipt.nonce)return true;
        for(std::size_t i=0;i<provisional_;++i)if(provisionals_[i].nonce==receipt.nonce)return true;
        return false;
    }

    [[nodiscard]] bool quiescent(std::span<const Lease> leases) const noexcept {
        if(leases.empty() || inflight_!=0)return false;
        for(const auto& lease:leases) {
            if(!valid(lease))return false;
            const auto receipt=capture(lease);if(!receipt)return false;
            for(std::size_t i=0;i<queued_;++i)if(events_[i].nonce==receipt.nonce)return false;
            for(std::size_t i=0;i<provisional_;++i)if(provisionals_[i].nonce==receipt.nonce)return false;
        }
        return true;
    }

    [[nodiscard]] bool overflow() const noexcept {return overflow_;}
    void observation_lost() noexcept {overflow_=true;}

private:
    struct Binding final {Lease lease{};std::uint64_t nonce{};};
    struct Queued final {Event event{};std::uint64_t nonce{};};
    struct Provisional final {Event event{};std::uint64_t nonce{};};
    struct Inflight final {std::uint64_t nonce{},floor{};};

    [[nodiscard]] static bool valid(const Lease& lease) noexcept {
        return lease.activity && lease.source.valid() && lease.bubble<=63
            && lease.source.activity==lease.activity.sessionId
            && lease.source.incarnation==lease.activity.incarnation.value
            && lease.source.source.type==1;
    }
    [[nodiscard]] static bool valid_event(const Event& event) noexcept {
        if(event.kind==Kind::sourceRecreated)
            return event.lease.discardStreamedReplicas && event.sourceHandle!=UINT32_MAX
                && event.previousSourceHandle!=UINT32_MAX && event.previousSourceHandle!=event.sourceHandle
                && event.actor.owner==event.lease.source
                && event.actor.actor==UINT32_MAX && event.actor.entity==UINT32_MAX;
        return (event.kind==Kind::admitted || event.kind==Kind::died || event.kind==Kind::retired)
            && event.actor.valid() && event.sourceHandle!=UINT32_MAX
            && event.actor.owner==event.lease.source;
    }
    [[nodiscard]] static bool valid_provisional(const Event& event) noexcept {
        return event.kind==Kind::admitted && valid(event.lease)
            && event.actor.owner==event.lease.source && event.actor.actor!=UINT32_MAX
            && event.sourceHandle!=UINT32_MAX;
    }
    [[nodiscard]] bool current(Receipt receipt) const noexcept {
        if(!receipt)return false;
        for(std::size_t i=0;i<used_;++i)
            if(bindings_[i].nonce==receipt.nonce && bindings_[i].lease==receipt.lease)return true;
        return false;
    }
    [[nodiscard]] bool allocate(std::uint64_t& nonce) noexcept {
        if(nextNonce_==UINT64_MAX)return false;
        nonce=++nextNonce_;return true;
    }
    void erase_event(std::size_t index) noexcept {
        for(std::size_t i=index+1;i<queued_;++i) events_[i-1]=events_[i];--queued_;
    }
    void erase_provisional(std::size_t index) noexcept {
        for(std::size_t i=index+1;i<provisional_;++i) provisionals_[i-1]=provisionals_[i];--provisional_;
    }
    void erase_creation(std::size_t index) noexcept {creations_[index]=creations_[--inflight_];}

    std::array<Binding,kBindingCapacity> bindings_{};std::size_t used_{};
    // One frame drains 64 entries. The open-world request budget admits at most
    // 384 retained requests; conservative native fanout bounds three births and
    // nine lifecycle events per request.
    std::array<Queued,kEventCapacity> events_{};std::size_t queued_{};
    std::array<Provisional,kProvisionalCapacity> provisionals_{};std::size_t provisional_{};
    std::array<Inflight,kCreationCapacity> creations_{};std::size_t inflight_{};
    std::uint64_t nextNonce_{};bool overflow_{};
};

// Synchronized bridge. Native hooks copy and qualify records before calling it;
// the authoritative activity update drains them without invoking native code.
[[nodiscard]] bool bind(const Lease&) noexcept;
[[nodiscard]] RenewResult renew(const Lease&,const Lease&) noexcept;
void release(ActivityInstanceKey) noexcept;
[[nodiscard]] std::uint64_t epoch() noexcept;
[[nodiscard]] bool pending(ActivityInstanceKey) noexcept;
[[nodiscard]] bool pending_lease(const Lease&) noexcept;
[[nodiscard]] bool quiescent(std::span<const Lease>) noexcept;
[[nodiscard]] Lease lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept;
[[nodiscard]] Receipt capture(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot,std::uint32_t generation) noexcept;
[[nodiscard]] Receipt capture(const Lease&) noexcept;
[[nodiscard]] Creation begin_creation() noexcept;
[[nodiscard]] Creation begin_creation(bool& observing) noexcept;
[[nodiscard]] StageResult stage(Creation,const Event&,Receipt&) noexcept;
void cancel(Creation) noexcept;
[[nodiscard]] bool provisional(Receipt,const Event&) noexcept;
/** Validates and consumes the exact provisional; external owners can suppress mailbox publication. */
[[nodiscard]] AdmitResult admit(Receipt,const Event&,bool publish=true) noexcept;
void unbind(const Lease&) noexcept;
[[nodiscard]] bool submit(const Event&,Receipt) noexcept;
[[nodiscard]] bool submit(const Event&,std::uint64_t epoch) noexcept;
void observation_lost() noexcept;
[[nodiscard]] std::size_t drain(ActivityInstanceKey,std::span<Event>,bool& overflow) noexcept;
}
