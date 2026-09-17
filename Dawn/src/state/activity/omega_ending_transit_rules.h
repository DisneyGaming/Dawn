#pragma once

#include <cstdint>
#include "omega_ending_rules.h"

namespace dawn::state::activity::omega_ending_transit {

/** C72CE0's native request tuple. This token is independent of membership+350. */
struct Teleport final {
    std::int8_t state{};
    std::uint8_t token{};
    std::int32_t sliceSetIndex{-1};
    std::uint32_t sliceSetHash{};
    friend constexpr bool operator==(const Teleport&, const Teleport&) = default;
};

/** The assembler validates the exact mission before entering this policy. */
struct Scope final {
    omega_ending::Token ending{};
    std::uint64_t memberKey{};
    bool validatedOmega{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        return validatedOmega && ending.valid() && memberKey != 0 && memberKey != UINT64_MAX;
    }
    friend constexpr bool operator==(const Scope&, const Scope&) = default;
};

struct Target final {
    std::int32_t region{-1};
    std::uint32_t spawnSet{};
    [[nodiscard]] constexpr bool valid() const noexcept {
        // The native current-world readiness getter accepts only packed0..511.
        return region >= 0 && region < 512 && spawnSet != 0 && spawnSet != UINT32_MAX;
    }
    friend constexpr bool operator==(const Target&, const Target&) = default;
};

/** Raw client message22 fields, before any host publication override. */
struct Observation final {
    Teleport local{};
    std::int32_t currentRegion{-1};
    bool hasTeleport{}, hasRegion{};
};

struct Authority final {
    Teleport host{};
    bool publish{}, arrived{}, complete{};
};

/** E22250 compares each PAH peer's D4 synchronization byte against the
 * teleport command token. Region-transition tokens are a separate domain.
 * Only acknowledge the host after native local sync and target-host readiness;
 * this does not report world arrival or advance the teleport transaction. */
[[nodiscard]] constexpr bool host_synchronization_ready(Authority command, Observation native,
    bool hasSynchronizationToken, std::uint8_t synchronizationToken,
    bool hostReady, std::int32_t advertisedRegion) noexcept {
    return command.publish && hostReady && hasSynchronizationToken
        && synchronizationToken==command.host.token && command.host.token!=0
        && advertisedRegion==command.host.sliceSetIndex
        && native.hasTeleport && native.hasRegion
        && native.currentRegion==command.host.sliceSetIndex
        && native.local.token==command.host.token
        && native.local.sliceSetIndex==command.host.sliceSetIndex
        && native.local.sliceSetHash==command.host.sliceSetHash
        && (native.local.state==2 || native.local.state==3
            || (command.complete && native.local.state==0));
}

enum class Phase : std::uint8_t { idle, requesting, releasing, complete };

/** Reconstructed host policy for the verified original C72CE0 client contract.
 * Caller owns locking and resets this object on run/epoch/member replacement.
 * There are no world writes, player coordinates, elapsed-time success or token
 * substitutions. A distinct nonzero command prevents accepting the idle
 * client's retained tuple as a receipt for this transaction.
 */
class Transaction final {
public:
    [[nodiscard]] constexpr bool begin(Scope scope, Target target,
                                       Observation native) noexcept {
        if (!scope.valid() || !target.valid()) { return false; }
        if (phase_ != Phase::idle) { return scope == scope_ && target == target_; }
        // Membership retains hasTeleport once any native message22 reports it.
        // Before the first host command, a client may never have published that
        // optional field: its untouched default tuple is valid bootstrap input.
        // An unreported nondefault tuple is not evidence of an idle client.
        if (native.local.state != 0 || (!native.hasTeleport && native.local != Teleport{})) { return false; }
        scope_ = scope;
        target_ = target;
        auto next = static_cast<std::uint8_t>(native.local.token + 1U);
        if (next == 0) { next = 1; }
        host_ = {1, next, target.region, target.spawnSet};
        phase_ = Phase::requesting;
        return true;
    }

    /** True only on the first exact native local3/current-target arrival. */
    [[nodiscard]] constexpr bool observe(Scope scope, Observation native) noexcept {
        if (scope != scope_ || !scope.valid() || !native.hasTeleport
            || native.local.token != host_.token
            || native.local.sliceSetIndex != target_.region
            || native.local.sliceSetHash != target_.spawnSet) { return false; }
        if (phase_ == Phase::requesting && native.local.state == 3
            && native.hasRegion && native.currentRegion == target_.region) {
            host_.state = 3;
            phase_ = Phase::releasing;
            arrived_ = true;
            return true;
        }
        if (phase_ == Phase::releasing && native.local.state == 0) {
            host_.state = 0;
            phase_ = Phase::complete;
        }
        return false;
    }

    [[nodiscard]] constexpr Authority authority(Scope scope) const noexcept {
        if (scope != scope_ || !scope.valid() || phase_ == Phase::idle) { return {}; }
        // Retain host0 after completion. Re-echoing a delayed local1/2 would
        // incorrectly ask an idle native client to replay the teleport.
        return {host_, true, arrived_, phase_ == Phase::complete};
    }
    [[nodiscard]] constexpr Phase phase() const noexcept { return phase_; }
    constexpr void reset() noexcept { *this = {}; }

private:
    Scope scope_{};
    Target target_{};
    Teleport host_{};
    Phase phase_{};
    bool arrived_{};
};

} // namespace dawn::state::activity::omega_ending_transit
