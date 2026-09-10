#pragma once

#include <cstdint>

namespace sunrise::state::activity::coo {
/** Retains native actor feedback within one spawn incarnation. The mission owns the run lease. */
struct CombatantState final {
    friend bool operator==(const CombatantState&,const CombatantState&)=default;
    std::uint32_t spawnRevision{}, programRevision{}, deliveryRevision{};
    std::uint8_t programState{}, actorQuery{};
    std::int8_t deliveryState{-1};
    bool identified{}, hasProgramRevision{}, hasProgramState{}, hasDeliveryRevision{}, hasActorQuery{};

    /** Clears dependent levels on detach or replacement; accepts sparse same-actor updates. */
    template<class Delta> void merge(const Delta& delta) noexcept {
        if (!delta.snapshotValid || delta.detached) { *this={}; return; }
        if (delta.hasSpawnRevision && (!identified || spawnRevision!=delta.spawnRevision)) {
            *this={}; spawnRevision=delta.spawnRevision; identified=true;
        }
        if (!identified) { return; }
        if (delta.hasProgramRevision) {
            if (!hasProgramRevision || programRevision!=delta.programRevision) {
                hasProgramState=false;
            }
            programRevision=delta.programRevision; hasProgramRevision=true;
        }
        if (delta.hasProgramState) { programState=delta.programState; hasProgramState=true; }
        if (delta.hasDeliveryRevision) {
            deliveryRevision=delta.deliveryRevision; hasDeliveryRevision=true;
        }
        // The reflected delivery state is required in each delta, unlike its revision.
        deliveryState=delta.deliveryState;
        if (delta.hasActorQuery) { actorQuery=delta.actorQuery; hasActorQuery=true; }
    }
    [[nodiscard]] bool program(std::uint32_t spawn, std::uint32_t revision,
                               std::uint8_t state) const noexcept {
        return identified && spawnRevision==spawn && hasProgramRevision && hasProgramState
            && programRevision==revision && programState==state;
    }
    [[nodiscard]] bool delivery(std::uint32_t spawn, std::uint32_t revision,
                                std::int8_t state) const noexcept {
        return identified && spawnRevision==spawn && hasDeliveryRevision
            && deliveryRevision==revision && deliveryState==state;
    }
};
} // namespace sunrise::state::activity::coo
