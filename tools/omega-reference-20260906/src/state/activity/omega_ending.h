#pragma once
#include "omega_ending_rules.h"
#include "omega_ending_transit_rules.h"
#include "lifecycle_generation.h"

namespace dawn::state::activity::omega_ending {
/** Encounter-owner final-death boundary, never inferred from position/time. */
[[nodiscard]] bool request(Token token) noexcept;
/** Explicit Insert-menu preview, allowed only in the opening Lighthouse before
 * leaving for the Forest. Does not publish encounter kills or progression. */
[[nodiscard]] bool preview_available() noexcept;
[[nodiscard]] bool request_preview() noexcept;
/** The exact movie must have been observed active and then finished. These APIs
 * track one native activity-selection attempt, never claim destination arrival. */
[[nodiscard]] Token handoff_request() noexcept;
[[nodiscard]] bool claim_handoff(Token token) noexcept;
void note_handoff_result(Token token, bool queued) noexcept;
/** Supplies native type-6 commands and the terminal authored slice-state request. */
[[nodiscard]] Authority authority(std::uint64_t run, std::uint64_t now) noexcept;
/** The client's `cinematic_skip` incident (msg 19, row kCinematicSkipIncident) while the
 * movie is playing. Publishes the stop authority; the native inactive receipt completes it. */
[[nodiscard]] bool request_skip(std::uint64_t run) noexcept;
/** Read-only pending identity for the exact native roster-removal observer.
 * A completed dialogue requests retirement; querying never advances it. */
[[nodiscard]] Token retirement_request(std::uint64_t run) noexcept;
/** Called only after the exact old Lair roster has completed native cleanup.
 * Publication/transport acknowledgements must not call this API. */
[[nodiscard]] bool observe_retirement(Token token) noexcept;
/** Called inside the exact native 80F47BCB controller tick/apply ownership. */
void observe(Token token, std::uint32_t revision, bool active, bool resourceReady) noexcept;
/** Exact native host-transaction completion and current-region receipt only. */
[[nodiscard]] bool observe_arrival(Token token, std::int32_t region) noexcept;
struct TransitInput final {
    ActivityInstanceKey activity{};
    std::uint64_t run{}, memberKey{};
    bool validatedOmega{};
    omega_ending_transit::Observation native{};
};
/** Projection of copied native message22 facts into host message12. The caller
 * validates Omega's exact activity destination and never writes this result
 * back into the client mirror. Native transitionToken remains untouched. */
[[nodiscard]] omega_ending_transit::Authority project_transit(const TransitInput& input) noexcept;
/** Acknowledged, stationary membership still needs the new host teleport tuple. */
[[nodiscard]] bool membership_due(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept;
/** Only after the complete membership frame and its State revision commit. */
void note_membership_published(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t now) noexcept;
/** A reset also invalidates late native receipts and a previous ending token. */
void reset() noexcept;
} // namespace dawn::state::activity::omega_ending
