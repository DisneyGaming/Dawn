# Reset ownership and stale-observation rejection

## Original gap

MISSING.md §0.1 and §1.3 describe incomplete recovery and lifecycle ownership. Our implementation supplies a narrower foundation: observations and publications belong to specific lifetimes, and resetting a lifetime cannot make old observations valid again.

## Attempt ownership

The shared owner combines a nonzero run ID with a nonzero generation. Beginning an attempt clears the active owner and completion state, then reserves a range above a retained generation high-water mark. The new owner starts above the previous range.

Reset clears the lease and completion bit but preserves the high-water mark. Reusing the same run ID still produces a later generation. Old death, controller, and readiness observations fail the new owner's checks.

The default reservation leaves room for preparation and a following creation generation. The current owner can extend its reserved range through a later revision without changing its identity or completion state. Extension requires the exact owner and a supported maximum. Exhaustion refuses the operation instead of wrapping to a previously used identity.

Completion accepts only the current valid owner and only once. The completion publication retains that owner; it is not a global flag any delayed callback can set.

## Network ownership

The activity layer separately types generations for connection, authentication, binding, activity incarnation, host region, activation, roster graph, and publication. An activity instance includes its session ID and incarnation, because session IDs/storage can be reused.

Staged roster work checks the exact binding and activity instance again at settlement. Replacement logic also distinguishes an activity lease owned by the connection from an activity it merely joined. This prevents an unrelated binding from being treated as the owner responsible for retirement.

## Example and limits

Attempt A admits an actor at generation 5. After reset, attempt B reuses the run ID but starts above the reserved range. A delayed generation-5 death cannot satisfy B. No timestamp heuristic is required.

The high-water mark is retained in memory, not persisted across process restarts. This is not durable checkpoint storage, automatic reconnect, late-join restoration, or host migration.

Landmarks: Sunrise/src/state/activity/coo/lifecycle_service.h; state/activity/lifecycle_generation.h; server/bap/internal.h. Generation and retirement regression sources exist but were not run for these docs.
