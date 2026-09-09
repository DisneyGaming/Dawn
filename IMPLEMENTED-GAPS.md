# Implemented gaps compared with MISSING.md

Focused on native tracking, ownership, roster delivery, and related infrastructure. Lua/executor work and mission walkthroughs are excluded. These findings come from the September 7 source inspection; this document is not a fresh validation of every September 9 change.

## Native death tracking — §10.7

Our population tracking retains authenticated actor identities and their death observations. Encounter clearance can require deaths from the admitted population, rather than treating elapsed time or proximity as a kill. A native boss-death observer also exists.

This supplies the gameplay capability that the document describes as unresolved. It does not add a new network death event or establish every slot's Sense semantics.

Source: `Sunrise/src/state/activity/coo/population_service.h`; `Sunrise/src/client/hooks/bootflow/omega_mission_health.inl`.

## Reset ownership and stale-event rejection — §0.1, §1.3

Owned lifecycle state uses generations to distinguish attempts. Reset clears owned state while retaining a generation high-water mark. Earlier attempts' observations cannot satisfy the current attempt. Activity infrastructure also distinguishes connection, binding, activity incarnation, region, and publication generations.

This is implemented reset/ownership infrastructure. It does not establish durable checkpoints, reconnect restoration, late join, or host migration.

Source: `Sunrise/src/state/activity/coo/lifecycle_service.h`; `Sunrise/src/state/activity/lifecycle_generation.h`.

## Activity-roster publication ownership — §1.2–1.3, §7

Roster publication checks the current activity, binding, and publication generation. It stages delivery state and counters, commits them when publication succeeds, and rolls them back when the staged body is discarded. Stale staged publications are rejected.

This addresses local publication ownership and rollback. Successful publication is not proof of client-side effect acknowledgement.

Source: `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_push.cpp`.

## Consistent region, membership, and roster delivery — §1.2–1.3

A retained transition snapshot contains the corresponding global state, membership, advertisement, roster, and authority-grant data. Required notifications are built as an all-or-nothing bundle. Failure rolls back the staged bundle; deferred-publication state retains the outstanding update.

This is concrete coordination/retry infrastructure beyond independent message codecs. It does not close general multiplayer handoff or late-join recovery.

Source: `Sunrise/src/server/bap/region_lineage.h`; `Sunrise/src/server/bap/encrypted/activity_transaction/activity_transaction_notifications.cpp`.

## Character/orbit roster refreshes — §6

Equipment changes produce an incremental family-3 character record plus the changed roster at one incremented revision. An equipped socket change refreshes the character record without unnecessarily replacing the roster list. Corresponding family-0 appearance changes are also staged alongside inventory changes.

These are real refresh paths. They do not prove every blank-banner symptom or unknown appearance field is fixed.

Source: `Sunrise/src/server/bap/encrypted/push/snapshot/roster_snapshot.cpp`; `Sunrise/src/server/bap/encrypted/queuez/queuez_outcome_staging.cpp`.

## Native object and destruction ownership — §0.1

Object initialization uses generation-scoped bindings and qualified entity/controller observations. Destructible handling distinguishes initialization, vulnerability, and confirmed destruction of the same owner. Stale generations and mismatched controllers are rejected.

The implementation exists for mapped native objects; this is not a general server-side entity replication or physics system.

Source: `Sunrise/src/state/activity/coo/object_service.h`; `Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md`.

## Native movement and cinematic observations — §10.5–10.6

Mapped native paths can observe movement/animation progress and camera activation/completion. Cinematic completion and retirement can depend on native observations rather than merely accepting a command.

This is a partial capability: arbitrary actor-command movement and unrelated cinematic readiness cases remain separate gaps.

Source: `Sunrise/src/client/hooks/bootflow/omega_mission_motion.inl`; `Sunrise/docs/COO-EXECUTOR.md` (historical native acceptance evidence).

## Matchmaking descriptor lookup — §0.1

Advertisement state can retain a descriptor and return it for a locate-session request. This goes beyond exclusively empty or ID-only replies.

A general matchmaking queue and session-search service are still missing.

Source: `Sunrise/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp`.

## Roster distinctions that matter

- Package-derived activity groups, slot identities, bubble scoping, and player-SOID binding exist, but MISSING.md already acknowledges parts of that implementation. They are not all new differences.
- Family-0/3 equipment appearance encoding exists. The specific `appearanceValue` field mentioned in MISSING.md is still not read by the inspected encoder.
- Family-2 social roster remains an empty-snapshot fallback. Activity and character roster work does not implement the social roster.
- Family-4 companion/duplicate snapshot behavior remains; the refresh work does not fix that subscription discrepancy.

Comparison basis: our inspected source versus your friend's written descriptions, not a direct comparison with their repository. No tests or game runs were performed for this write-up.
