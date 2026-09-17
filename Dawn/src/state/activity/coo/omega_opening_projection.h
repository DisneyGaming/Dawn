#pragma once
#include "omega_opening.h"

namespace dawn::state::activity::coo::omega::opening {
// The observer owns unrelated diagnostics and publication acknowledgements.
// Preserve their accepted reset semantics while migrating only opening policy.
template<class Observer>
[[nodiscard]] Changes update_observer(Observer& observer, std::uint64_t binding) noexcept {
    const auto changes = observer.omegaOpeningExecutor.update(binding);
    if (changes.reset) {
        const auto retained = observer.omegaOpeningExecutor;
        observer = {};
        observer.omegaOpeningExecutor = retained;
    }
    if (changes.ready) {
        observer.omegaSceneHandoffArmed = false;
        observer.omegaSceneCompleted = false;
        observer.omegaPortalTransportConfirmed = false;
        observer.omegaForestEntranceAuthorityPublished = false;
    }
    const auto state = observer.omegaOpeningExecutor.authority();
    observer.omegaRosterReady = state.roster;
    observer.omegaOpeningTriggered = state.requested;
    observer.omegaForestEntranceTriggered = state.entrance;
    return changes;
}
template<class Snapshot>
void project(const Run& run, Snapshot& snapshot) noexcept {
    const auto state = run.authority();
    if (state.failed) { snapshot.omegaSceneAuthority = false; }
    snapshot.omegaIkoraPortalRequested = snapshot.omegaSceneAuthority && state.requested;
    snapshot.omegaIkoraLatticeReleased = snapshot.omegaSceneAuthority && state.released;
    snapshot.omegaPortalEntry = snapshot.omegaIkoraLatticeReleased;
}
} // namespace dawn::state::activity::coo::omega::opening
