#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "../../../state/activity/destination/definition.h"
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../state/activity/membership/definition.h"

namespace sunrise::server::runtime::activity::membership_transit {

using Owner = state::activity::ActivityInstanceKey;
namespace membership = state::activity::membership;

/** The largest number of authored destinations retained by one service. */
inline constexpr std::size_t kDestinationCapacity = 16;
/** The largest number of members retained by one activity incarnation. */
inline constexpr std::size_t kMemberCapacity = 16;
/** Native host state one requests a transition. */
inline constexpr std::int8_t kRequestingState = 1;
/** Native local state three confirms arrival at the authored destination. */
inline constexpr std::int8_t kArrivedState = 3;

/** One trusted destination selected by the server's authored activity definition. */
struct Destination final {
    std::uint32_t id{};
    std::int32_t region{};
    std::uint32_t spawn{};

    friend constexpr bool operator==(const Destination&, const Destination&) noexcept = default;
};

/** A member-scoped native receipt, including the request identity that produced it. */
struct Receipt final {
    Owner owner{};
    std::uint64_t boot{};
    std::uint64_t requestId{};
    std::uint64_t member{};
    state::activity::membership::TeleportState local{};
    std::int32_t actualRegion{state::activity::membership::kAbsentRegionIndex};
};

/** A trusted command requesting one member's next authored destination. */
struct Command final {
    Owner owner{};
    std::uint64_t boot{};
    std::uint64_t expectedRevision{};
    std::uint64_t requestId{};
    std::uint32_t destinationId{};
    std::uint64_t member{};
};

/** The lifecycle of one member's retained native host tuple. */
enum class Phase : std::uint8_t { idle, requesting, arrived, released };

/** Results of a request admission attempt. */
enum class RequestResult : std::uint8_t {
    accepted,
    duplicate,
    stale,
    unsupported,
    not_ready,
    busy,
    exhausted,
};
using Result = RequestResult;

/** Results of observing one synthetic/native receipt. */
enum class ObservationResult : std::uint8_t {
    ignored,
    observed,
    arrived,
    released,
    duplicate,
    capacity,
};

/** The side-effect-free native projection for one member. */
struct Projection final {
    std::uint64_t member{};
    std::uint64_t requestId{};
    Destination destination{};
    state::activity::membership::TeleportState host{};
    Phase phase{Phase::idle};
    bool present{};
    bool arrived{};
    bool released{};
};

/**
 * Bounded, reusable native membership teleport state.
 *
 * begin() copies and validates authored destinations. A member must first be observed at native
 * local state zero. request() then publishes host state one; only observe() can advance the host
 * to state three or state zero. project() never changes state, and all receipts remain scoped by
 * owner, boot, member, and request identity.
 */
class Service final {
    struct Member final {
        std::uint64_t member{};
        std::uint64_t requestId{};
        Destination destination{};
        membership::TeleportState host{};
        std::uint8_t observedToken{};
        Phase phase{Phase::idle};
        bool used{};
        bool hasObservedToken{};
    };

public:
    /**
     * Checks one authored destination against the native membership representation.
     * @param destination Authored destination to validate.
     * @return True when its id, region, and spawn can be represented and used.
     */
    [[nodiscard]] static constexpr bool valid(const Destination& destination) noexcept {
        return destination.id != 0
            && destination.region >= 0
            && destination.region <= membership::kMaximumRegionIndex
            && destination.spawn != 0
            && destination.spawn != state::activity::destination::kAbsentSpawnSetHash
            && destination.spawn != (std::numeric_limits<std::uint32_t>::max)();
    }

    /**
     * Checks a complete bounded authored destination span, including id uniqueness.
     * @param destinations Authored destinations to validate.
     * @return True when the span is nonempty, bounded, and wholly valid.
     */
    [[nodiscard]] static constexpr bool valid(
        std::span<const Destination> destinations) noexcept {
        if (destinations.empty() || destinations.size() > kDestinationCapacity) {
            return false;
        }
        for (std::size_t index = 0; index < destinations.size(); ++index) {
            if (!valid(destinations[index])) {
                return false;
            }
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (destinations[previous].id == destinations[index].id) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Binds one immutable owner/boot and copies its trusted destination definitions.
     * @param owner Activity incarnation that owns this transaction.
     * @param boot Server boot lifetime that scopes the transaction.
     * @param destinations Authored destinations copied before the service becomes live.
     * @return True only when admission succeeds; a live service cannot be rebound.
     */
    [[nodiscard]] bool begin(Owner owner, std::uint64_t boot,
        std::span<const Destination> destinations) noexcept {
        if (owner_ || !owner || boot == 0 || !valid(destinations)) {
            return false;
        }
        destinations_ = {};
        for (std::size_t index = 0; index < destinations.size(); ++index) {
            destinations_[index] = destinations[index];
        }
        destinationCount_ = destinations.size();
        owner_ = owner;
        boot_ = boot;
        revision_ = state::activity::membership::kInitialRevision;
        return true;
    }

    /**
     * Observes one member's native local tuple or advances its scoped transaction.
     *
     * A requestId of zero is accepted only to seed a previously unseen member with local state
     * zero. Once a member has a request, every later receipt must carry that request's identity.
     * @param receipt Native local receipt modeled without wire bytes.
     * @return A one-shot arrived/released edge, or a non-advancing observation result.
     */
    [[nodiscard]] ObservationResult observe(const Receipt& receipt) noexcept {
        if (!owns(receipt.owner, receipt.boot) || !usable_member(receipt.member)) {
            return ObservationResult::ignored;
        }

        Member* member = find(receipt.member);
        if (!member) {
            if (receipt.requestId != 0 || receipt.local.state != 0) {
                return ObservationResult::ignored;
            }
            member = allocate(receipt.member);
            if (!member) {
                return ObservationResult::capacity;
            }
            member->observedToken = receipt.local.token;
            member->hasObservedToken = true;
            return ObservationResult::observed;
        }

        if (member->phase == Phase::idle) {
            // Repeated pre-request state-zero reports are safe observations. They are the only
            // time an unscoped receipt may replace the token used to start a transaction.
            if (receipt.requestId != 0 || receipt.local.state != 0) {
                return ObservationResult::ignored;
            }
            member->observedToken = receipt.local.token;
            member->hasObservedToken = true;
            return ObservationResult::observed;
        }

        if (receipt.requestId != member->requestId
            || receipt.local.token != member->host.token
            || receipt.local.sliceSetIndex != member->destination.region
            || receipt.local.sliceSetHash != member->destination.spawn) {
            return ObservationResult::ignored;
        }

        member->observedToken = receipt.local.token;
        member->hasObservedToken = true;
        if (member->phase == Phase::requesting && receipt.local.state == kArrivedState
            && receipt.actualRegion == member->destination.region) {
            member->host.state = kArrivedState;
            member->phase = Phase::arrived;
            return ObservationResult::arrived;
        }
        if (member->phase == Phase::arrived && receipt.local.state == 0) {
            member->host.state = 0;
            member->phase = Phase::released;
            return ObservationResult::released;
        }
        if (member->phase == Phase::arrived || member->phase == Phase::released) {
            return ObservationResult::duplicate;
        }
        return ObservationResult::observed;
    }

    /**
     * Admits a new request or returns a non-mutating idempotency/lifecycle result.
     * @param command Owner, revision, request, destination, and member command identity.
     * @return Admission status; accepted is the only result that publishes host state one.
     */
    [[nodiscard]] RequestResult request(const Command& command) noexcept {
        if (!owns(command.owner, command.boot) || command.expectedRevision != revision_) {
            if (owns(command.owner, command.boot) && same_request(command)) {
                return RequestResult::duplicate;
            }
            return RequestResult::stale;
        }
        if (command.requestId == 0 || !usable_member(command.member)) {
            return RequestResult::unsupported;
        }

        Member* member = find(command.member);
        if (!member || !member->hasObservedToken) {
            return RequestResult::not_ready;
        }
        if (command.requestId < member->requestId) {
            return RequestResult::stale;
        }
        if (command.requestId == member->requestId) {
            return same_request(command) ? RequestResult::duplicate : RequestResult::stale;
        }
        if (member->phase != Phase::idle && member->phase != Phase::released) {
            return RequestResult::busy;
        }

        const Destination* destination = find_destination(command.destinationId);
        if (!destination) {
            return RequestResult::unsupported;
        }
        if (revision_ == (std::numeric_limits<std::uint64_t>::max)()) {
            return RequestResult::exhausted;
        }

        member->requestId = command.requestId;
        member->destination = *destination;
        member->host = {kRequestingState, next_token(member->observedToken), destination->region,
            destination->spawn};
        member->phase = Phase::requesting;
        ++revision_;
        return RequestResult::accepted;
    }

    /**
     * Returns the current native projection for one owner-scoped member without mutation.
     * @param owner Activity incarnation expected by the caller.
     * @param boot Boot lifetime expected by the caller.
     * @param member Member identity to project.
     * @return The member's host tuple, or an absent projection for a foreign/unknown member.
     */
    [[nodiscard]] Projection project(Owner owner, std::uint64_t boot,
        std::uint64_t member) const noexcept {
        if (!owns(owner, boot) || !usable_member(member)) {
            return {};
        }
        const Member* state = find(member);
        if (!state) {
            return {};
        }
        return {state->member, state->requestId, state->destination, state->host, state->phase,
            state->phase != Phase::idle, state->phase == Phase::arrived
                || state->phase == Phase::released, state->phase == Phase::released};
    }

    /**
     * Clears the whole service only for its exact owner and boot lifetime.
     * @param owner Activity incarnation being torn down.
     * @param boot Boot lifetime being torn down.
     * @return True when the exact live owner was cleared; no arrival is fabricated.
     */
    [[nodiscard]] bool cancel(Owner owner, std::uint64_t boot) noexcept {
        if (!owns(owner, boot)) {
            return false;
        }
        reset();
        return true;
    }

    /** Alias for lifecycle callers that call teardown rather than cancel. */
    [[nodiscard]] bool teardown(Owner owner, std::uint64_t boot) noexcept {
        return cancel(owner, boot);
    }

    /** Clears an unowned or already-cancelled service without producing a native result. */
    void reset() noexcept { *this = {}; }

    /** @return The immutable owner, or an absent key after teardown. */
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    /** @return The immutable boot lifetime, or zero after teardown. */
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    /** @return The current command revision, or zero before begin. */
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

private:
    [[nodiscard]] bool owns(Owner owner, std::uint64_t boot) const noexcept {
        return owner_ && owner == owner_ && boot != 0 && boot == boot_;
    }

    [[nodiscard]] static constexpr bool usable_member(std::uint64_t member) noexcept {
        return member != 0 && member != membership::kInvalidOpaqueSoid;
    }

    [[nodiscard]] static constexpr std::uint8_t next_token(std::uint8_t token) noexcept {
        const auto next = static_cast<std::uint8_t>(token + 1U);
        return next == 0 ? state::activity::membership::kInitialTransitionToken : next;
    }

    [[nodiscard]] Member* find(std::uint64_t member) noexcept {
        for (auto& candidate : members_) {
            if (candidate.used && candidate.member == member) {
                return &candidate;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Member* find(std::uint64_t member) const noexcept {
        for (const auto& candidate : members_) {
            if (candidate.used && candidate.member == member) {
                return &candidate;
            }
        }
        return nullptr;
    }

    [[nodiscard]] Member* allocate(std::uint64_t member) noexcept {
        for (auto& candidate : members_) {
            if (!candidate.used) {
                candidate = {};
                candidate.member = member;
                candidate.used = true;
                return &candidate;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Destination* find_destination(std::uint32_t id) const noexcept {
        for (std::size_t index = 0; index < destinationCount_; ++index) {
            if (destinations_[index].id == id) {
                return &destinations_[index];
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool same_request(const Command& command) const noexcept {
        const Member* member = find(command.member);
        return command.owner == owner_ && command.boot == boot_ && command.requestId != 0
            && usable_member(command.member) && member && member->member == command.member
            && member->requestId == command.requestId
            && member->destination.id == command.destinationId;
    }

    Owner owner_{};
    std::uint64_t boot_{};
    std::uint64_t revision_{};
    std::array<Destination, kDestinationCapacity> destinations_{};
    std::array<Member, kMemberCapacity> members_{};
    std::size_t destinationCount_{};
};

} // namespace sunrise::server::runtime::activity::membership_transit
