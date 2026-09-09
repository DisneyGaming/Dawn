# Implemented Gaps Summary & Capability Comparison

> **Source Documents Consolidated:**
> - `IMPLEMENTED-GAPS.md` (September 7, 2026 inspection)
> - `IMPLEMENTED-VS-MISSING.md` (Capability comparison vs. MISSING.md)
>
> **Detailed Feature Notes:**
> See deep-dive documentation in [`deep-dives/`](./deep-dives/README.md) for individual architectural breakdowns (Notes 01–10).

---

## 1. Executive Summary

This document summarizes the capabilities implemented in our current Sunrise source compared with the build state described in `MISSING.md`. The baseline comparison was performed September 7, 2026, comparing our inspected source tree against written descriptions.

### Important Boundaries & What Remains Open
**Do NOT mark these gaps closed:**
- Family-2 social roster still falls back to an empty snapshot.
- Family-4 companion/duplicate snapshot behavior remains.
- General server-side physics and server-authoritative AI remain unimplemented.
- Durable player persistence is not established.
- Full matchmaking queue and session-search services remain missing.
- Quality of Service (QoS) and voice infrastructure are incomplete.
- Network reconnect, late join, and host migration / handoff are not established.
- Complete roster-field and destination coverage are not established.

*Verification Note:* Current source and call paths were inspected; no fresh live playthrough or package regeneration was performed for this summary.

---

## 2. Implemented Capabilities in Sunrise Source

### 2.1. Executable Mission Policy (§0.1)
Mission graphs actively control population schedules, encounter dependencies, objectives, dialogue, devices, and completion gates. JSON loading and validated native capability bindings exist, rather than merely typed intents without mission rules.
- **Sources:** `Sunrise/src/state/activity/coo/executor.h`, `coo/mission_script.cpp`, `Sunrise/scripts/`

### 2.2. Reusable Mission Services (§0.1)
Shared services cover enemy readiness, object initialization, destructible states, scene milestones, event-relative timers, objective/marker lifecycle, and mission completion.
- **Sources:** `Sunrise/src/state/activity/coo/`, `Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md`

### 2.3. Native Death Tracking (§10.7)
Our population tracking retains authenticated actor identities and their death observations. Encounter clearance can require confirmed deaths from the admitted population, rather than treating elapsed time or proximity as a kill. A native boss-death observer also exists.
- **Scope & Limits:** This supplies the gameplay capability that `MISSING.md` describes as unresolved. It does not add a new wire-level network death event or establish every slot's Sense semantics.
- **Deep Dive:** [`deep-dives/01-NATIVE-DEATH-TRACKING.md`](./deep-dives/01-NATIVE-DEATH-TRACKING.md)
- **Sources:** `Sunrise/src/state/activity/coo/population_service.h`, `Sunrise/src/client/hooks/bootflow/omega_mission_health.inl`

### 2.4. Reset Ownership & Stale-Event Rejection (§0.1, §1.3)
Owned lifecycle state uses generations to distinguish attempts. Reset clears owned state while retaining a generation high-water mark. Observations from earlier attempts cannot satisfy the current attempt. Activity infrastructure also distinguishes connection, binding, activity incarnation, region, and publication generations. An implemented mission also has recorded user-confirmed death/retry acceptance.
- **Scope & Limits:** This is implemented reset/ownership infrastructure. It does not establish durable checkpoints, reconnect restoration, late join, or host migration.
- **Deep Dive:** [`deep-dives/02-RESET-OWNERSHIP.md`](./deep-dives/02-RESET-OWNERSHIP.md)
- **Sources:** `Sunrise/src/state/activity/coo/lifecycle_service.h`, `Sunrise/src/state/activity/lifecycle_generation.h`, `Sunrise/docs/COO-EXECUTOR.md`

### 2.5. Transactional Activity-Roster Publication Ownership (§1.2–1.3, §7)
Roster publication validates the current activity, binding, and publication generation, rejecting stale work. Delivery state and counters are staged until publication succeeds; if discarded, staged bodies roll back cleanly.
- **Scope & Limits:** Local publication ownership and rollback are addressed. Successful publication is not proof of client-side effect acknowledgement.
- **Deep Dive:** [`deep-dives/03-ACTIVITY-ROSTER-PUBLICATION.md`](./deep-dives/03-ACTIVITY-ROSTER-PUBLICATION.md)
- **Sources:** `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_push.cpp`

### 2.6. Consistent Region, Membership, and Roster Delivery (§1.2–1.3)
A single immutable transition snapshot retains matching global state, membership, advertisement, roster, and authority-grant data. Required notifications are assembled as an all-or-nothing bundle with rollback support and retained deferred-publication state.
- **Scope & Limits:** Concrete coordination/retry infrastructure beyond independent message codecs. Does not close general multiplayer handoff or late-join recovery.
- **Deep Dive:** [`deep-dives/04-REGION-MEMBERSHIP-COORDINATION.md`](./deep-dives/04-REGION-MEMBERSHIP-COORDINATION.md)
- **Sources:** `Sunrise/src/server/bap/region_lineage.h`, `Sunrise/src/server/bap/encrypted/activity_transaction/activity_transaction_notifications.cpp`

### 2.7. Character & Orbit Roster Refreshes (§6)
Equipment changes produce an incremental family-3 character record plus the changed roster at one incremented revision. An equipped socket change refreshes the character record without unnecessarily replacing the roster list. Corresponding family-0 appearance changes are also staged alongside inventory changes.
- **Scope & Limits:** Real refresh paths exist, but they do not prove every blank-banner symptom or unknown appearance field is resolved.
- **Deep Dive:** [`deep-dives/05-CHARACTER-ROSTER-REFRESHES.md`](./deep-dives/05-CHARACTER-ROSTER-REFRESHES.md)
- **Sources:** `Sunrise/src/server/bap/encrypted/push/snapshot/roster_snapshot.cpp`, `Sunrise/src/server/bap/encrypted/queuez/queuez_outcome_staging.cpp`

### 2.8. Native Object & Destruction Ownership (§0.1)
Object initialization uses generation-scoped bindings and qualified entity/controller observations. Destructible handling distinguishes initialization, vulnerability, and confirmed destruction of the same owner. Stale generations and mismatched controllers are rejected.
- **Scope & Limits:** Implemented for mapped native objects; this is not a general server-side entity replication or physics system.
- **Deep Dive:** [`deep-dives/06-NATIVE-OBJECT-OWNERSHIP.md`](./deep-dives/06-NATIVE-OBJECT-OWNERSHIP.md)
- **Sources:** `Sunrise/src/state/activity/coo/object_service.h`, `Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md`

### 2.9. Native Movement & Cinematic Orchestration (§10.5–10.6)
Mapped native paths can observe movement/animation progress, camera activation/readiness, and cinematic completion. Cinematic completion, destination handoff, and retirement can depend on native observations rather than merely accepting a command.
- **Scope & Limits:** Partial capability for mapped content; arbitrary actor-command movement and unrelated cinematic readiness remain separate gaps.
- **Deep Dive:** [`deep-dives/07-NATIVE-MOVEMENT-AND-CINEMATICS.md`](./deep-dives/07-NATIVE-MOVEMENT-AND-CINEMATICS.md)
- **Sources:** `Sunrise/src/client/hooks/bootflow/omega_mission_motion.inl`, `Sunrise/src/state/activity/coo/omega_ending_definition.h`, `Sunrise/docs/COO-EXECUTOR.md`

### 2.10. Matchmaking Descriptor Lookup (§0.1)
Advertisement state can retain a descriptor and return it for a `locateSession` request, going beyond exclusively empty or ID-only replies.
- **Scope & Limits:** A general matchmaking queue and session-search service are still missing.
- **Deep Dive:** [`deep-dives/08-MATCHMAKING-DESCRIPTOR-LOOKUP.md`](./deep-dives/08-MATCHMAKING-DESCRIPTOR-LOOKUP.md)
- **Sources:** `Sunrise/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp`

---

## 3. Roster & Encoding Distinctions

### 3.1. Package-Derived Activity Rosters
Our build extracts registry groups and slot descriptors, preserves slot types/flags/indices, publishes global and bubble-scoped groups, and resolves the player's full character SOID.
- *Distinction:* `MISSING.md` already acknowledges parts of that implementation. They are not all new differences from the external build.
- **Deep Dive:** [`deep-dives/09-PACKAGE-DERIVED-ACTIVITY-ROSTERS.md`](./deep-dives/09-PACKAGE-DERIVED-ACTIVITY-ROSTERS.md)
- **Sources:** `Sunrise/src/client/content/scenarios/scenario_roster_build.cpp`, `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`

### 3.2. Character Appearance Encoding
Family-0 and Family-3 equipment appearance encoding exists (render entries, stats, ability buckets, hashes, and perk banks).
- *Distinction:* The specific `appearanceValue` field mentioned in `MISSING.md` is still not read by the inspected encoder.
- **Deep Dive:** [`deep-dives/10-CHARACTER-APPEARANCE-ENCODING.md`](./deep-dives/10-CHARACTER-APPEARANCE-ENCODING.md)
- **Sources:** `Sunrise/src/middleware/datagen/character_record/character_record_encoder.cpp`

### 3.3. Remaining Roster Gaps
- **Family-2 Social Roster:** Remains an empty-snapshot fallback. Activity and character roster work does not implement the social roster.
- **Family-4 Companion/Duplicate Snapshot:** Discrepancies in subscription behavior remain unaddressed.
