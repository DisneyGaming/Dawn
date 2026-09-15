#pragma once
#include "native_population_events.h"

namespace sunrise::state::activity::open_world_members {
namespace events = native_population;

struct MemberRef final {
    std::uint32_t resource{UINT32_MAX}, kind{};
    std::int64_t offset{};
    friend bool operator==(const MemberRef&, const MemberRef&) = default;
};

// Internal correlation values are never serialized. This sidecar cannot create
// an authoritative admission, hold a renewal open, or fail a population ledger.
struct Observation final {
    events::Receipt binding{};
    coo::PopulationActor actor{};
    std::uint32_t sourceHandle{UINT32_MAX};
    MemberRef member{};
    bool conflicted{};
};
enum class Intake : std::uint8_t { stored, duplicate, conflict, invalid, evicted };
enum class Take : std::uint8_t { found, missing, busy, inactive };
namespace detail {
template<class Try> [[nodiscard]] bool try_bounded(Try attempt) noexcept {
    for (unsigned i = 0; i < 8; ++i) if (attempt()) return true;
    return false;
}
}

template<std::size_t Capacity> class Mailbox final {
    static_assert(Capacity > 0 && Capacity <= 256);
public:
    [[nodiscard]] Intake offer(const Observation& value, std::size_t* discarded = nullptr) noexcept {
        if (discarded) *discarded = 0;
        const auto& lease = value.binding.lease;
        if (!value.binding || !lease.activity || !value.actor.valid()
            || value.actor.owner != lease.source || lease.source.source.type != 1
            || lease.source.activity != lease.activity.sessionId
            || lease.source.incarnation != lease.activity.incarnation.value
            || value.sourceHandle == UINT32_MAX || lease.bubble > 63)
            return Intake::invalid;
        // A newer authenticated binding cannot share pending authoritative
        // receipts with its predecessor. Remove stranded predecessor sidecars.
        for (std::size_t i = 0; i < used_;) {
            const auto& prior = values_[i].binding;
            if (prior.lease.activity == lease.activity
                && prior.lease.source.source == lease.source.source
                && prior.lease.source.run == lease.source.run && prior.nonce < value.binding.nonce) {
                erase(i); if (discarded) ++*discarded;
            } else ++i;
        }
        for (std::size_t i = 0; i < used_; ++i) {
            auto& prior = values_[i];
            if (!same_actor(prior, value.binding, value.actor, value.sourceHandle)) continue;
            if (prior.conflicted || value.conflicted || prior.member != value.member) {
                prior.conflicted = true;
                return Intake::conflict;
            }
            return Intake::duplicate;
        }
        if (used_ == Capacity) {
            // Diagnostic overload must fail forward. A lost/aborted sidecar
            // cannot permanently deny all later observations a slot.
            values_[eviction_++ % Capacity] = value;
            if (discarded) ++*discarded;
            return Intake::evicted;
        }
        values_[used_++] = value;
        return Intake::stored;
    }

    [[nodiscard]] bool take(events::Receipt binding, const events::Event& event,
        Observation& output) noexcept {
        output = {};
        if (!binding || event.kind != events::Kind::admitted || binding.lease != event.lease)
            return false;
        for (std::size_t i = 0; i < used_; ++i) {
            if (!same_actor(values_[i], binding, event.actor, event.sourceHandle)) continue;
            output = values_[i];
            erase(i);
            return true;
        }
        return false;
    }

    void release(ActivityInstanceKey owner) noexcept {
        for (std::size_t i = 0; i < used_;)
            if (values_[i].binding.lease.activity == owner) erase(i); else ++i;
    }
    void release_source(const coo::PopulationOwner& owner) noexcept {
        for (std::size_t i = 0; i < used_;)
            if (values_[i].binding.lease.source == owner) erase(i); else ++i;
    }
    void clear() noexcept { values_ = {}; used_ = eviction_ = 0; }
    [[nodiscard]] std::size_t size() const noexcept { return used_; }
private:
    static bool same_actor(const Observation& value, events::Receipt binding,
        const coo::PopulationActor& actor, std::uint32_t sourceHandle) noexcept {
        return value.binding == binding && value.actor == actor && value.sourceHandle == sourceHandle;
    }
    void erase(std::size_t index) noexcept {
        values_[index] = values_[--used_];
        values_[used_] = {};
    }
    std::array<Observation, Capacity> values_{};
    std::size_t used_{}, eviction_{};
};

// The server census lifecycle is the only enabling authority. Native callbacks
// use try-only locks and never perform filesystem I/O or settings lookup here.
void start() noexcept;
void stop() noexcept;
[[nodiscard]] bool enabled() noexcept;
void capture(events::Receipt, const events::Event&, MemberRef) noexcept;
[[nodiscard]] Take take(events::Receipt, const events::Event&, Observation&) noexcept;
void discard(events::Receipt, const events::Event&) noexcept;
void release(ActivityInstanceKey) noexcept;
void release_source(const coo::PopulationOwner&) noexcept;
[[nodiscard]] std::uint64_t losses() noexcept;
}
