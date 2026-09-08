# Our build vs. your friend's MISSING.md

Capabilities implemented in our current Sunrise source that are missing or only partial in the build described by MISSING.md. Compared September 7, 2026. This is a brief capability comparison, not a claim that every related gap is closed.

## Implemented in our build

- **Executable mission policy (§0.1):** Mission graphs actually control population schedules, encounter dependencies, objectives, dialogue, devices, and completion gates. JSON loading and validated native capability bindings exist, rather than only typed intents without mission rules. Sources: `Sunrise/src/state/activity/coo/executor.h`, `coo/mission_script.cpp`, `Sunrise/scripts/`.

- **Reusable mission services (§0.1):** Shared services cover enemy readiness, object initialization, destructible states, scene milestones, event-relative timers, objective/marker lifecycle, and mission completion. Sources: `Sunrise/src/state/activity/coo/` and `Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md`.

- **Native death tracking (§10.7):** Population services retain authenticated actor identities and death observations and use them to release encounter gates. This supplies mission death tracking without requiring a new wire-level death event. Source: `Sunrise/src/state/activity/coo/population_service.h`.

- **Movement and cinematic orchestration, for mapped content (§10.5–10.6):** Native movement, animation, camera readiness/completion, cinematic retirement, and destination handoff have working implementations. This is not a general solution for every actor command or cinematic. Sources: `Sunrise/src/client/hooks/bootflow/omega_mission_motion.inl`, `Sunrise/src/state/activity/coo/omega_ending_definition.h`.

- **Reset/retry ownership, partially (§0.1):** Generation-scoped resets and stale-observation rejection exist; an implemented mission also has recorded user-confirmed death/retry acceptance. This does not establish network reconnect, late join, or durable checkpoint persistence. Sources: `Sunrise/src/state/activity/coo/lifecycle_service.h`, `Sunrise/docs/COO-EXECUTOR.md`.

- **Transactional activity-roster publication (§1.2–1.3, §7):** Sends validate activity/binding/publication generations and reject stale work. Delivery counters and state are staged until publication commits; discarded sends roll back. Source: `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_push.cpp`.

- **Coordinated region/membership/roster updates (§1.2–1.3):** One immutable transition snapshot holds matching global state, membership, advertisement, roster, and authority-grant data. Required notifications form an all-or-nothing bundle, with rollback and retained deferred-publication state. Sources: `Sunrise/src/server/bap/region_lineage.h`, `encrypted/activity_transaction/activity_transaction_notifications.cpp`.

- **Incremental character/orbit roster refreshes (§6):** Equipment changes publish the changed family-3 character record and roster at one incremented revision. Equipped socket changes refresh the character record; corresponding family-0 appearance updates are also staged. Sources: `Sunrise/src/server/bap/encrypted/push/snapshot/roster_snapshot.cpp`, `encrypted/queuez/queuez_outcome_staging.cpp`.

- **State-backed matchmaking lookup, partially (§0.1):** Advertisement descriptors can be retained and returned by `locateSession`, beyond exclusively empty/id-only responses. A general matchmaking queue/search service remains missing. Source: `Sunrise/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp`.

## Already present, but not a clean difference from your friend's build

- **Package-derived activity rosters:** Our build extracts registry groups and slot descriptors, preserves slot types/flags/indices, publishes global and bubble-scoped groups, and resolves the player's full character SOID. MISSING.md already acknowledges parts of this, so the whole roster builder should not be counted as a new difference. Sources: `Sunrise/src/client/content/scenarios/scenario_roster_build.cpp`, `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`.
- **Character appearance encoding:** Family-0/3 records encode equipment render entries, stats, ability buckets, hashes, and perk banks. However, the document's narrower claim about the specific `appearanceValue` field remains true: this encoder does not read it. Source: `Sunrise/src/middleware/datagen/character_record/character_record_encoder.cpp`.

## Do not mark these gaps closed

Family-2 social roster still falls back to an empty snapshot. Family-4 companion/duplicate snapshot behavior remains. General server-side physics/AI, durable player persistence, full matchmaking, QoS, voice, network reconnect/late join/host handoff, and complete roster-field/destination coverage are not established by the implementations above.

Verification: current source and call paths inspected; no new build, tests, or game run performed. Existing live acceptance applies to its recorded candidates. Newer shared-service integrations still have documented live-validation gaps. Your friend's code was not supplied, so the comparison uses their document's descriptions.
