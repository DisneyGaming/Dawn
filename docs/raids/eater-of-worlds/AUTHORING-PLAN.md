# Authoring plan and unresolved work

This plan follows the current Sunrise mission split: Lua owns story choices; native bindings identify assets and capabilities; shared services own execution, receipts, and lifecycle. It deliberately stops before implementation.

## Phase A: close launch and residency identity

1. Register one of the three proven `raid_envy_v310` public activity identities only after mapping its intended variant meaning. Preserve ordinals 536–538 and hashes `0xB8218A8C`, `0x81029D0A`, and `0x303AF7C6`; do not label them normal/prestige from position alone.
2. Prove the initial destination, player-control handoff, arrival bubble, region/slice, and spawn set. Existing repository notes explicitly identify the bubble-7 spawn-set binding as missing; do not infer it from the `leviathan` stem or the spaceflight names.
3. Record which of the shared Leviathan bubbles the activity actually streams before mouth and belly. A scenario bubble list is an availability graph, not phase order.
4. Confirm the required map/activity package residency. Use the package list in the JSON as reachability evidence, then validate actual runtime residency rather than loading every adjacent Leviathan package.

Acceptance gate: a fresh activity arrives under the Eater identity, gives player control at the intended start, reports the correct map-global bubble, and retains native package/object owners.

## Phase B: publish the native roster catalog

1. Extend authored-group discovery or provide an Eater-specific package-proven catalog for the `0x0221` groups. The current cache's same-package rule omits them because the scenario is in `0x01A4`.
2. Preserve every original registry key, slot type, slot index, descriptor tag/offset, component class, sense schema, and auth schema from the evidence JSON.
3. Preserve host-only declarations as unresolved. Do not create descriptors for the 171 missing Argos slots, 131 missing barrier slots, 144 missing platform slots, or the smaller group gaps.
4. Publish only the groups required for the active bubble/phase. Do not arm all 891 client descriptors at arrival.
5. Bind the 75 squad sources through their original tactical rows. Validate creation, typed health, AI owner, tactical registry/slot/row, and real death before reporting readiness or clearance.

Acceptance gate: the intended encounter groups appear with exact immutable layouts; delayed admission works; optional populations can survive where allowed; no synthetic death or readiness receipts are used.

## Phase C: write the mission graph in narrow vertical slices

Build one connected section at a time:

1. Opening arrival and **Explore the Leviathan**.
2. Platform path 1 through 4, checkpoint lights, holdout activation, authentic Loyalist clear, exit grate, and **Escape the reactor** / **Defeat the Loyalists**.
3. False floor, crossing entry, airlock, piston, ejection tube, seven rings, traversal checkpoint, and **Delve deeper**.
4. Barrier engagement, three-side populations, relic/fire interactions, six oracle spawners, genuine barrier completion, and **Break the barrier**.
5. Argos reveal scene, shield sections, missiles, Harpy swarms, damage phase, detainment, alpha-strike interrupt, boss death, loot, dialogue completion, and **Destroy Argos**.

For each slice, Lua should declare commands, authenticated observations, alternative conditions, objective/marker choice, permitted delays, and completion gate. Native bindings should expose only proven operations and receipts.

## Phase D: presentation ownership

1. Bind objective events by their seven exact hashes. Preserve the two separate “Explore the Leviathan” rows.
2. Bind all six dialogue selectors, then identify which native scene or host service owns each exchange. Bank row order is not story order.
3. Decode the complete cast, event prerequisites, and completion outputs for scene graphs `0x80F444FB` and `0x80F44500` before requesting them.
4. Keep queue acceptance, native submission, conversation start, conversation completion, and scene completion as separate observations.
5. Start relative timing from an acknowledged native event. Do not use request time as a proxy.

Acceptance gate: fast traversal does not drop or duplicate dialogue; Argos appears and animates through the intended native owner; final speech completes before mission completion.

## Phase E: specialized mechanics

Prefer existing `population_service`, `object_service`, `scene_orchestration`, `dialogue_service`, `objective_service`, `event_timeline`, and `lifecycle_service`. Add mission-specific native bindings for mechanics whose verified contract is not shared:

- falling-platform arming, occupancy, warning, sink, reset, and path completion
- ring/ejection traversal and thundering-wall checkpoint updates
- cranium pickup, elemental charge, ammo, fire interaction, and target damage
- oracle/missile scheduling and wipe cancellation
- shield-node convergence and damage-window authority
- Argos immunity, damage buff/debuff, detainment release, alpha-strike interrupt, and weak-point state
- checkpoint/wipe reset of objects, populations, behaviors, dialogue deduplication, and phase latches

The six behavior roots should remain under the client interpreter. Their purpose is to prove native assets and predicates that must be allowed to self-activate through ordinary ownership, not to define replacement server bytecode.

## Concrete unresolved gaps

- Player-facing meanings of public activity ordinals 536–538, the native launch-selection path, and the authoritative initial arrival/spawn set.
- The bubble-7 spawn-set binding and actual start geometry.
- Cross-package publication for the `0x0221` authored groups.
- Exact host-only slot meanings and whether any are required server authority objects.
- Trigger geometry and threshold values for platform goals, airlocks, piston, rings, fire stations, engagement sensors, and traversal phase tasks.
- Device state values/revisions for doors, checkpoint lights, platforms, shields, oracles, missiles, relics, fire interactables, teleporter, and boss geometry.
- Exact live selection counts, formation rows, timing, reinforcements, optional-versus-required deaths, and reset policy for every squad.
- Native scene casts, event IDs, prerequisite masks, parent/child handoff, conversation ownership, and completion receipts.
- Dialogue-selector call sites and story selection policy, including repeat-run variants.
- Element-to-side mapping for altar, cliffs, and grove.
- Cranium charge/ammo values and oracle/missile timers; all currently quoted values are community references.
- Shield convergence rule, damage-phase duration, unstable-energy rule, detainment release condition, alpha-strike weak-point selection, stun duration, and failure reset.
- Root-specific behavior lifecycle thresholds for the thunder wall and alpha-strike owners, despite the general per-frame submitter being known.
- Same-room checkpoint recovery, wipe behavior, encounter re-entry, late receipt rejection, and mission-complete publication order.

## Validation plan

Use a clean process and the intended activity selection. Validate one mechanic at a time with native diagnostics, then run the complete route.

- Opening: identity, arrival, control, objective, and no premature encounter arming.
- Platforms: both path occupancy and failure, delayed teammates, checkpoint restart, holdout scheduling, optional survivors, and authentic final clear.
- Traversal: early/repeated triggers, airlock ordering, piston failure, all seven rings, and checkpoint update.
- Barrier: each elemental side, interrupted and completed charge, wrong-element rejection, delayed enemy admission, each oracle wipe, and reset.
- Argos: intro scene/cast, all shield sections, missed missile, repeated damage cycles, detainment, alpha-strike interrupt/failure, boss death, loot, and final dialogue.
- Lifecycle: wipe, retry, stale generations, late deaths, duplicate scene/dialogue requests, teardown, and a second fresh run.

Passing parser/unit tests establishes authoring consistency. It does not replace a fresh live run that proves visibility, interaction, audio, native animation, wipe/reset, and final completion.
