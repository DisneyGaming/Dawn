# Eater of Worlds: comprehensive solo reconstruction plan

Date: 13 September 2026  
Target: Destiny 2 build 86657, Sunrise, `raid_envy_v310`  
Status: planning and package research; no Eater gameplay acceptance is claimed  
Working identifier: `eater_of_worlds` (proposed implementation name)

Inspection update: [reactor findings](LIVE-REACTOR-20260913.md) prove successful bubble-7 / slice-56 arrivals at the first-path goal (`0x1E8DBF89`) and reactor approach (`0xAD98065D`), with geometry joins for the four path goals and start trigger. The [barrier/Argos capture](LIVE-ARGOS-20260913.md) proves bubble-6 / slice-48 arrival at `0x68C397B7` and readable authored definitions for all 335 Argos, 301 barrier, and 22 belly-traversal descriptors. Combined availability coverage is 886/891 known client descriptors; the five remaining belong to the outer entrance. This initial collection pass is complete, with no separate traversal load needed for descriptor coverage. Original entrance acceptance, active encounter controls, host-only scheduling, and gameplay acceptance remain open. The user operates all computer controls; further investigation uses files, logs, and read-only game data.

## 1. Purpose and execution order

Outer-entrance exception from live testing: bubble 2 / slice 16 / spawn `0x8BA80878` failed on mission run 7 (user-reported BIRD). The log confirms the slice loaded, then native `sobject` creation failed and prerequisite 35 became unavailable before entering gameplay. The run-9 [read-only diagnosis](ENTRANCE-BIRD-DIAGNOSIS.md) strongly identifies startup entity-ID starvation: 150 `sobject` records consume all available IDs; native error 12 occurs with only 151/1,024 record slots occupied, and a refill appears after failure. Fix ID availability across initial instantiation and validate arrival before accepting this tuple. The exact failing object and full successful entrance population remain unknown. [Original failure evidence](../../../build/coo/eater-live-inspection-20260913/entrance-bird-failure/failure-evidence.json).

[Offline entrance recovery](ENTRANCE-RECOVERY.md) is complete: all five known entrance client definitions, two trigger volumes, typed wrappers, groups, and scenario/bubble metadata are exported as 27 complete native tags with byte-verified hashes. The five definitions were already in the inventory; they were missing live verification, not package data. Continue entrance diagnosis from these saved sources without requiring another collection-only load. Live coverage remains 886/891 and gameplay gates remain open.

Build a complete single-player Eater of Worlds using its recovered native content and the current Sunrise mission infrastructure. Preserve the route, elemental cranium interactions, platform traversal, barrier encounter, Argos mechanics, presentation, and meaningful failure conditions. Adapt the requirements that depend on several players acting at once.

The user explicitly chose **encounter-by-encounter implementation in raid order**:

1. Establish the baseline and only the infrastructure needed for arrival.
2. Implement and accept arrival and the entrance.
3. Implement and accept the reactor platforms and Loyalist holdout.
4. Implement and accept the mouth-to-belly traversal.
5. Implement and accept Break the Barrier, including the cranium system.
6. Implement and accept Argos, reusing the accepted cranium system.
7. Implement and accept the ending, then validate the entire raid.

For each section, follow **recover -> bind -> implement -> test offline -> install a coherent candidate -> playtest -> fix -> accept**. Later encounters may be inventoried to understand dependencies, but they do not become the active implementation milestone before the current section passes its gate. In particular, do not start with an Argos prototype ahead of the reactor and barrier.

This document is the detailed execution plan. [AUTHORING-PLAN.md](AUTHORING-PLAN.md) remains the original research handoff; [evidence/native-inventory.json](evidence/native-inventory.json) remains the extracted identity source. This plan does not turn an unknown field into a recovered binding.

The current request authorizes writing this plan. The commands, source changes, installation, and live experiments below are future implementation work. On continuation, use the user's actual implementation instructions and existing session authorization; do not invent recurring approval gates for routine work already authorized.

## 2. Navigation

- [Scope, fidelity, and solo policy](#3-scope-fidelity-and-solo-policy)
- [Evidence and recovered starting point](#4-evidence-and-recovered-starting-point)
- [Architecture and integration](#5-architecture-and-integration)
- [Recovery method and records](#6-recovery-method-and-records)
- [Milestone 0: baseline](#7-milestone-0-baseline-and-development-setup)
- [Milestone 1: arrival](#8-milestone-1-arrival-and-entrance)
- [Milestone 2: reactor and holdout](#9-milestone-2-reactor-platforms-and-loyalist-holdout)
- [Milestone 3: traversal](#10-milestone-3-mouth-to-belly-traversal)
- [Milestone 4: barrier](#11-milestone-4-break-the-barrier)
- [Milestone 5: Argos](#12-milestone-5-argos)
- [Milestone 6: ending](#13-milestone-6-ending-and-rewards)
- [Lifecycle and checkpoints](#14-lifecycle-checkpoints-and-recovery)
- [Population and solo balance](#15-population-and-solo-balance)
- [Presentation](#16-objectives-dialogue-scenes-and-music)
- [Validation, packaging, and delivery](#17-validation-packaging-and-delivery)
- [Troubleshooting](#18-troubleshooting-order)
- [Gap register](#19-gap-register)
- [Work packages and handoff](#20-work-packages-and-handoff)
- [Reference index](#21-reference-index)

## 3. Scope, fidelity, and solo policy

### 3.1 Required result

- One human player can launch the activity and finish every required encounter.
- Progression requires the intended player actions and authentic native results.
- Required native scenery, collision, enemies, boss animation, dialogue, objectives, and ending presentation work.
- Death and encounter failure return the player to a usable checkpoint with a clean attempt.
- Completed encounters remain completed during the same activity session.
- The final accepted run needs no debug skipping, manually injected completion, held memory values, or developer intervention.
- Existing missions and their launch behavior continue to work.

There is no completion-date or first-pass guarantee in this plan. The earlier probability estimate was a subjective assessment, not a schedule or acceptance criterion. Discovery difficulty is measured by remaining contracts and failed gates.

### 3.2 What is retained

- The recovered entrance and intended traversal route.
- Four reactor platform paths and the Loyalist encounter, with native platform hazards.
- False floor, doors, airlock, piston environment, ejection sequence, and seven ring objects.
- Cranium collection, elemental charging, carrying, aiming, ammunition, and matching-element damage.
- Barrier targets and their failure pressure.
- Argos shield preparation and convergence, damage windows, attacks, detainment, wipe interruption, and real death.
- Native dialogue choices and authored scenes where their contracts can be recovered.

Preserving an object does not require preserving a six-player staffing rule. Solo changes must still expose the original information and demand the original class of action.

### 3.3 Working solo policy

These are implementation proposals derived from the discussion, not recovered retail rules or already accepted tuning values:

1. **Participation:** use the one real active player. Do not create fake player slots, fake occupancy, or imaginary teammate actions.
2. **Reactor:** retain the player's valid ordered platform activations long enough to replace simultaneous staffing. Keep movement hazards and checkpoint failure. Determine physical platform persistence separately from logical progress.
3. **Barrier:** allow enough cranium staging and target time for one person to perform all required jobs. Preserve elemental rejection and genuine target destruction.
4. **Argos shield:** permit sequential convergence by retaining valid target placement for a bounded window. The player must still charge, carry, aim, and place all required elements.
5. **Detainment:** provide a reliable action the trapped player can perform to escape. First investigate native damage from inside the cage. It is not yet proved supported.
6. **Wipe interruption:** retain the recovered weak-point action and target requirement where feasible; tune target health and repositioning time for one player.
7. **Combat:** tune encounter-specific simultaneous pressure and health. Preserve enemy roles, native AI, and attack patterns.
8. **Failure:** retain encounter wipes and useful checkpoints. A timeout must not silently award success.

Earlier conversation examples such as a 15/20/30-second shield hold, a 90-second dropped cranium lifetime, a two-minute mine window, or a target number of damage phases are **tuning hypotheses**. Select values only after measuring the real interactions. Do not bake these conversational examples into native evidence.

### 3.4 Design choices that must not slip in unnoticed

- Do not add the previously brainstormed new final stand unless separately requested.
- Do not invent a Psion platform-unlocking mechanic from the earlier discussion; require evidence or an explicit design decision.
- Do not assume one cranium can destroy exactly one target. Recover ammunition and damage first.
- Do not assume every ring is mandatory for progression. Recover whether rings control a bonus reward, a checkpoint, a traversal result, or a required gate.
- Do not assume six oracle spawners mean six waves, or 27 missile objects mean 27 simultaneous attacks.
- Do not turn all squads in an inventory into mandatory kill gates.
- Do not label the three native activity rows Normal/Prestige merely from their order.
- Do not introduce companions, multiplayer support, prestige variants, new encounter puzzles, or a global difficulty rewrite as prerequisites for the solo version.

If a native constraint forces a meaningful change to the player's mechanic, record the observed limitation and concrete alternative. Resolve that specific choice with the user if it changes the agreed experience; continue independent work on the current encounter in the meantime.

## 4. Evidence and recovered starting point

### 4.1 Evidence labels

Attach one or more labels to every material binding or behavioral claim:

- **PACKAGE:** exact installed tag, schema, field, reference, or serialized value.
- **ARCHIVE VERIFIED:** supplied decompilation artifact joined byte-for-byte to installed content.
- **NATIVE CODE:** verified executable function/consumer with the image identity, calling convention, and relevant data flow recorded.
- **REFERENCE:** documented player behavior from an identified walkthrough or guide; include timestamp when available.
- **OFFLINE TEST:** parser, graph, protocol, consumer emulation, or replay result; state its boundary.
- **LIVE OBSERVED:** result measured in a named game process and run with matching build identity.
- **USER CONFIRMED:** visible/audible behavior confirmed by the player in that run.
- **SOLO DESIGN:** an intentional reconstruction or solo rule.
- **UNKNOWN:** unresolved identity, semantics, timing, or acceptance.

An implemented command is not automatically applied. Applied native state is not automatically visible animation. A visible animation is not automatically an authenticated encounter completion.

### 4.2 Confirmed identity and package topology

The current inventory records:

- Scenario `0x80B49E7A`, class `0x80809994`, name `raid_envy_v310`.
- Package-definition hash `0x3B50D933` and scenario size 1,462 bytes.
- Public activity ordinals 536, 537, 538 with hashes `0xB8218A8C`, `0x81029D0A`, `0x303AF7C6`.
- All three display “Leviathan, Eater of Worlds”; their variant meanings remain unresolved.
- Mouth: `raid_wing_envy_mouth`, bubble hash `0x217C510E`, map-global index 26, entry `0x80B49E79`, registry `0x80C46065`.
- Belly: `raid_wing_envy_belly`, bubble hash `0x8CAD7143`, map-global index 25, entry `0x80B49E78`, registry `0x80C43A40`.
- Eight scenario bubbles. A scenario-local ordinal is not a map-global index, region, slice, or spawn-set identifier.
- 19 recovered groups, 891 resolved client descriptors, 75 squad sources, seven objective rows, six dialogue selectors, two scene graphs, and six verified Eater-linked behavior roots.

The version-54 evidence has zero authored-group counts for all eight bubbles. The scenario is in package `0x01A4`; the large Eater encounter groups are in `0x0221` and are omitted by the current catalog selection represented in that extraction. Recheck the current producer before implementing a correction: other work is active in this checkout.

Do not launch an original Leviathan `raid_gluttony_*` activity as a substitute for the Eater identity.

### 4.3 Encounter groups to recover in order

**Arrival/shared:**

- `0x80FA2C97`, key `0x24C67333`: 20/20 client descriptors, including player and lifetime globals.
- `0x80B49EB8`, key `0xA9E6185F`: 4/5 descriptors, entrance door, sensor, and music.
- `0x80B49EDA`, key `0xB270AC62`: 1/2 descriptors, Berth traversal task.
- `0x80C43A59`, key `0xD6E30062`: directive, music, and dialogue sensors.

**Reactor:**

- `0x80C43FF4`, key `0x686321C8`: 187 client descriptors / 331 declarations.
- 56 platform devices across paths of 13, 11, 13, and 19; 32 squad sources; 15 spawn regions.
- Path goals, occupancy filters, checkpoint lights, holdout objectives, grate, audio sequences, and death/loot objects.

**Traversal:**

- `0x80C43AAF`, key `0x5654D7FD`: false floor and crossing entrance.
- `0x80C43FFF`, `0x80C46009`, `0x80C46064`: mouth phase tasks and markers.
- `0x80C420D4`, key `0x93BF5E9D`: 22/44 descriptors, airlocks, wind, piston, ejection tube, rings, chest, sensor.
- `0x80C43A3E`, key `0xE34F861D`: belly traversal and thundering-wall checkpoint task.

**Barrier:**

- `0x80C43A1D`, key `0x91264981`: 301/432 descriptors.
- Six oracle spawner structures, 15 side squads, three side objectives, relic groups, nine fire stations, wipe actions, and barrier devices.

**Argos:**

- `0x80C42FA3`, key `0xE8D290A0`: 335/506 descriptors.
- Boss candidate `0x80C75F82`; three shield sections; 27 missile objects; 12 Harpy swarm squads; relic/fire structures; boss geometry, actions, scenes, and loot transition.

These are declaration and source counts. They are not safe publication limits, live actor counts, wave sizes, or proof of simultaneous residency.

### 4.4 Missing declarations and native behaviors

The large groups contain 144 unresolved reactor declarations, 131 barrier declarations, and 171 Argos declarations. Other groups have smaller gaps. Preserve those holes and their original slot identities. A missing client descriptor may describe host-only authority; it is not permission to manufacture a client object or compact the slots.

The six byte-verified behavior roots are:

- `0x80C75E63`: detain sphere.
- `0x80F42E5C`: thunder-wall safe zone.
- `0x80F42E67`: alpha-strike test and wipe timer.
- `0x80F42E68`: wipe damage.
- `0x80F44153`: production alpha strike.
- `0x80F44464`: detain finale damage over time.

Keep these under the native client interpreter. Investigate their owner lifecycle and required inputs. Do not assume all six should be explicitly triggered, that a test root is the production route, or that this list contains every relevant behavior in the raid. The current extractor finds six Eater-path-linked roots; indirect dependencies may need further traversal.

## 5. Architecture and integration

### 5.1 Ownership boundaries

**Lua owns mission decisions:** encounter order, supported operation requests, dependencies, population scheduling policy, selected dialogue/objectives, solo tuning exposed by registered capabilities, and completion conditions.

**Eater native catalog/bindings own identity:** exact registry/type/slot, definition offset, descriptor/runtime classes, placement, tactical source, scene cast, native state values, trigger geometry, and supported command/observation contracts.

**Shared services own execution and lifecycle:** population admission, object preparation, authenticated state application, scene orchestration, dialogue queueing, clocks, objectives, command retirement, and rejection of stale observations.

**Native adapters own engine interaction:** correct thread and ABI, native authority messages or supported operations, and authentic callbacks/readbacks. Extend existing boundaries where suitable; new helpers are not automatically new hooks.

**Solo mechanics own explicitly changed gameplay policy:** retained activation, staging lifetimes, shield convergence window, self-rescue, and tuning. Scope each change to the solo Eater activity and current attempt.

### 5.2 Existing code to inspect before adding infrastructure

- [Shared executor](../../../Sunrise/src/state/activity/coo/executor.h) and [Lua compiler](../../../Sunrise/src/state/activity/coo/script_lua.cpp).
- [Population service](../../../Sunrise/src/state/activity/coo/population_service.h), [object service](../../../Sunrise/src/state/activity/coo/object_service.h), and [native service bridge](../../../Sunrise/src/state/activity/coo/native_services.h).
- [Scene orchestration](../../../Sunrise/src/state/activity/coo/scene_orchestration.h), [scene authority](../../../Sunrise/src/state/activity/coo/native_scene_authority.h), and [cast authority](../../../Sunrise/src/state/activity/coo/native_scene_cast_authority.h).
- [Dialogue service](../../../Sunrise/src/state/activity/coo/dialogue_service.h), [event timeline](../../../Sunrise/src/state/activity/coo/event_timeline.h), and [lifecycle service](../../../Sunrise/src/state/activity/coo/lifecycle_service.h).
- [Native trigger support](../../../Sunrise/src/state/activity/coo/native_player_trigger.h) and [device authority](../../../Sunrise/src/state/activity/coo/native_device_authority.h).
- [Gateway bindings](../../../Sunrise/src/state/activity/gateway/bindings.h) for authored progression and native devices.
- [Garden World bindings](../../../Sunrise/src/state/activity/strike_bond/bindings.h), [boss cycle](../../../Sunrise/src/state/activity/strike_bond/boss_cycle.h), and [ending/wipe support](../../../Sunrise/src/state/activity/strike_bond/ending_wipe.h).

Inspect the actual compiled path and call sites. Earlier Omega work demonstrated that editing a similarly named inactive implementation can pass irrelevant tests without changing the installed encounter.

There is also population/world-object infrastructure under `Sunrise/src/server/runtime/activity/`. Compare its contracts with the CoO services before choosing a boundary. Do not attach two competing owners to the same Eater source.

### 5.3 Proposed file organization

These paths are planned, not existing functionality:

- `Sunrise/scripts/eater_of_worlds.lua`: authored phase graph and supported solo configuration.
- `Sunrise/src/state/activity/eater_of_worlds/catalog.h`: generated or verified package identities.
- `.../bindings.h`: registered capabilities, input bounds, and receipt policies.
- `.../controller.h`, `.../controller.cpp`, `.../runtime.h`, `.../runtime.cpp`: mission integration and current-attempt state.
- `.../authority.h`: Eater projections through supported native schemas.
- Mechanic helpers for reactor, traversal, relics, barrier, Argos, and checkpoints only where they improve ownership clarity.
- A mission roster adapter in the currently active publication path, following the existing project's organization.
- `Sunrise/unit/eater_*_tests.cpp` and matching projects for the contracts actually added.
- Recovery and validation tools under `tools/coo/` or this folder's `tools/`, with reproducible inputs and bounded outputs.

Do not create all these files up front as empty scaffolding. Introduce the smallest working set for the current milestone. Keep generated catalog data separate from hand-authored story and bindings.

### 5.4 Current Lua and executor constraints

The checked executor defines 32 steps per graph, eight commands per step, and a 128-entry receipt queue. The authoring guide also documents up to eight phases, 64 conditions, and bounded condition/data sizes; verify the compiler's current constraints before planning the final graph layout.

Lua builds immutable mission data at load time and then its VM closes. There is no gameplay coroutine loop or general live reload. A Lua `for` loop can construct records; it cannot watch the boss during gameplay.

A proposed top-level layout is six phases: `arrival`, `reactor`, `traversal`, `barrier`, `argos`, `ending`. These names and the layout require registration; they are not present capabilities. Treat platform paths, barrier rounds, and boss cycles as encounter substructure rather than consuming one top-level phase per repeat.

Before the reactor implementation exceeds graph limits, inspect existing composition/module support. Before the barrier and Argos repeat cycles, establish a supported repeat contract with per-cycle identity, cancellation, and reset. If no suitable contract exists, add a small tested shared extension with Lua-declared policy. Do not invent a `repeat`, `cancel`, or `.finished` DSL operation and assume the engine supports it. Do not hide the entire raid's story in a fixed C++ sequence merely to avoid graph limits.

Parameters such as health multipliers or fractional device values need the existing typed binding representation. The current Lua data interface is bounded and integer-oriented; do not pass unvalidated floating-point/pointer data through an imagined API.

### 5.5 Integration checklist

- [ ] Register the verified public activity and scenario association.
- [ ] Register the solo selection/preset with a verified arrival binding.
- [ ] Wire activity routing, controller lifecycle, roster projection, and callbacks.
- [ ] Add the Lua file to the actual loader and expected script lists.
- [ ] Add sources to the current build project.
- [ ] Add new tests to the current validation runner.
- [ ] Add the script to packaging, installation, validation-only checks, and rollback manifests.
- [ ] Keep unrelated activity selection, settings, packet defaults, and scripts unchanged unless a reviewed shared fix requires a change.

The current packager lists more scripts and the runner lists more suites than older documents' five-script/42-result baseline. Derive required validation from current source. Do not hard-code an old test count or claim an unsupported `--mission eater_of_worlds` packaging option exists.

## 6. Recovery method and records

### 6.1 Recover one mechanic through the complete chain

1. Locate the named descriptor in `native-inventory.json`.
2. Read its installed definition and preserve source tag, offset, class, and hash.
3. Follow controller, entity, scene, behavior, channel, or tactical references.
4. Map serialized fields to the native runtime consumer using the current executable/decompilation evidence.
5. Identify owner construction and residency prerequisites before the operation can work.
6. Identify the authority input, revision/sequence semantics, and exact application path.
7. Identify authentic output evidence: application, interaction, occupancy, damage, destruction, scene event, or death.
8. Record reset and cleanup behavior for interrupted and completed attempts.
9. Validate the schema/consumer offline where feasible.
10. Test the smallest visible interaction in the correct current encounter, then integrate it into progression.

If the client exposes the operation but no original host scheduling program is present, implement the host policy from the documented encounter and label it reconstruction. Further searching for unavailable original server code is not an acceptance requirement.

### 6.2 Binding record template

Create one record per important control in a future `BINDINGS.md` or structured equivalent:

- Binding ID and encounter.
- Evidence labels and present status.
- Desired player-visible behavior.
- Package/build/hash; registry key; type; slot; definition tag and offset.
- Definition class and runtime component class, recorded separately.
- Placement/geometry and coordinate space.
- Scene/behavior/tactical links and ownership construction path.
- Input state or native operation, native units, revision rules, and legal thread/ABI.
- Preconditions: resident package, group, source, entity, controller, player, scene, or combat phase.
- Output observation and how it authenticates the same source and attempt.
- Idle, active, completed, interrupted, reset, and retired behavior.
- Existing service reused or exact missing extension.
- Offline fixture and live capture references.
- Solo changes, remaining unknowns, and acceptance result.

Definition identity, runtime entity identity, and command identity serve different purposes. Preserve all three where required; a source name alone cannot authenticate a callback.

### 6.3 Beat contract template

Write this for every meaningful stage transition:

- **Preload:** what must already be visible or resident on approach.
- **Arm:** what enables gameplay interaction or damage.
- **Request:** which exact objects/populations/scenes are requested and at which cue.
- **Ready:** which acknowledged creations or state applications are prerequisites.
- **Advance:** which player/native event changes the stage.
- **Presentation:** objective, marker, speech, effects, and their playback owners.
- **Retire:** which sources and commands end; which survivors and dialogue persist.
- **Failure/reset:** checkpoint, state restoration, and invalidated receipts.

Do not collapse these into one convenient timer or room-clear condition.

### 6.4 Proposed evidence outputs

Keep the existing research files intact. Add these only as real findings become available:

- `BINDINGS.md`: verified operation contracts and unresolved fields.
- `SOLO-RULES.md`: selected policy, measured tuning, and reasons for deviations.
- `IMPLEMENTATION.md`: actual source integration and candidate status.
- `ACCEPTANCE.md`: section gates and exact accepted build/run references.
- `evidence/launch-bindings.json`, `reactor-bindings.json`, `traversal-bindings.json`, `relic-bindings.json`, `barrier-bindings.json`, `argos-bindings.json`: proposed structured extracts.
- `build/coo/eater-<milestone>-<candidate>/`: candidate manifests, logs, fixtures, binaries, and live evidence.

Every generated file needs a producer/version and input hashes. Preserve raw captures alongside interpretations. Do not overwrite a successful or failed candidate's historical evidence to describe later source.

## 7. Milestone 0: baseline and development setup

### 7.1 Baseline tasks

- [ ] Record branch, commit, relevant working changes, and active work in shared files.
- [ ] Identify the currently installed DLL, matching PDB, script set, and latest coherent installation receipt.
- [ ] Record the current executable and package identity. Use the verified unpacked image for runtime function analysis.
- [ ] Preserve an exact pre-change baseline without reverting unrelated work.
- [ ] Inspect current scenario catalog, script loader, launch path, and packaging lists.
- [ ] Verify the Eater research inputs still match the current cache and installed packages.
- [ ] Create a fresh evidence/candidate location for the current milestone.
- [ ] Record the intended one-player launch and which activity variant semantics remain unknown.
- [ ] Locate a continuous reference route if available; use the existing source index to start. Missing reference footage does not block package identity recovery.

This checkout already contains extensive uncommitted work, including shared launch, roster, lifecycle, and native service files. Do not clean/reset/stash it wholesale or overwrite a peer's work. Check the current contents immediately before changing a shared boundary; a previous tool read is not a frozen implementation baseline.

### 7.2 Reproduce extraction without replacing the original inventory

The extractor already exists. In a future recovery run, use a new output path so the original evidence remains comparable:

```powershell
python -B docs/raids/eater-of-worlds/tools/extract_eater_of_worlds.py --output build/coo/eater-baseline-<candidate>/native-inventory.json
```

Replace `<candidate>` with a unique run name and prepare that output directory as needed. Inspect the extractor's current arguments before use. It validates the pinned cache format and package/archive joins; if those fail, diagnose the changed input instead of removing the assertions.

Compare the new scenario, activity identities, groups, descriptors, objectives, dialogue, and behavior roots to the existing inventory. Investigate every structural difference before generating runtime bindings.

### 7.3 Catalog budget audit

Before publishing Eater content, inspect actual limits at every relevant layer:

- Per-group declared slot index and descriptor count; large Eater groups exceed 255 declarations.
- Type/slot encoding, sentinel handling, signedness, and source offsets.
- Number of simultaneously published groups, including shared/global/transition overlap.
- Population sources versus admitted actors, pending admissions, and native actor capacity.
- Packet size, serializer limits, receipt queue size, and publication cadence.
- Metadata lifetime when a group remains visible across a checkpoint or phase change.

The fact that the inventory has 19 groups does not establish that the current runtime can publish them all safely. Build a phase residency budget and publish only the active encounter's needed content. Do not silently raise a capacity or truncate slots to make a fixture pass.

### 7.4 Gate G0

- [ ] Baseline and ownership of changed files are known.
- [ ] Research inputs reproduce or all discrepancies are explained.
- [ ] The current runtime integration path is identified.
- [ ] No build/install assumptions rely on obsolete script or suite counts.
- [ ] Arrival work can begin without requiring later encounter mechanics.

Deliverable: a baseline record and a launch/catalog investigation list. This gate does not require full recovery of Argos or every host-only declaration.

## 8. Milestone 1: arrival and entrance

### 8.1 Player-visible outcome

Selecting the solo Eater entry loads the correct activity, lands the player at the intended start with control, presents the correct initial objective/dialogue, and allows the intended approach to the reactor.

### 8.2 Recovery tasks

- [ ] Follow each relevant public activity row into its launch selection and scenario records.
- [ ] Determine an appropriate native identity for the solo preset; preserve unresolved variant labels in research.
- [ ] Resolve initial destination, spawn set, transform, region, slice, scenario-local bubble, and map-global bubble separately.
- [ ] Resolve the known bubble-7 spawn-set gap; do not equate that local ordinal with map index 7.
- [ ] Verify whether Berth/shared Leviathan content belongs to the actual approach and which bubbles stream before the mouth.
- [ ] Recover door states, engagement sensor, entry volume, and traversal task completion.
- [ ] Recover initial objective and dialogue trigger/owner from the directive group.
- [ ] Identify package residency required for those exact sources.

### 8.3 Implementation tasks

- [ ] Add the verified Eater launch/preset and route it to its mission adapter.
- [ ] Publish common and entrance groups with original identities and valid authority.
- [ ] Correct cross-package catalog discovery using package-proven provenance, or provide a narrowly scoped Eater catalog if the generic producer cannot represent it yet.
- [ ] Retain declared/resolved distinctions and verify all slot layouts against the independent extract.
- [ ] Set the initial objective and briefing at authenticated arrival; do not add an arbitrary first-movement delay.
- [ ] Preload the entrance and visible approach before interaction arming.
- [ ] Let the verified entry trigger and native door application release traversal.
- [ ] Add an arrival checkpoint and clean activity teardown.

A registry-key match by itself is insufficient. The inventory notes a shared key published for a different object/layout; require the correct object provenance and exact layout rather than reusing that row.

### 8.4 Offline and live acceptance

- [ ] Activity selection resolves to the expected native Eater identity.
- [ ] Correct landing position, orientation, player control, and world collision.
- [ ] No wrong Leviathan activity, fallback spawn, unloaded destination, or debug teleport is needed.
- [ ] Opening objective and voice play once and remain correct when the player moves immediately.
- [ ] Entrance opens visibly and collision permits passage.
- [ ] Reactor approach loads the intended next content without arming the whole raid.
- [ ] Returning to selection and launching a second fresh activity does not reuse stale state.
- [ ] Existing affected activity launch tests pass.

**Gate G1:** fresh launch through the entrance works in the installed candidate. Record the build and run. Start reactor gameplay work only after this gate passes.

## 9. Milestone 2: reactor platforms and Loyalist holdout

### 9.1 Player-visible outcome

The player advances through all four native platform paths, reaches the recovered checkpoints, fights the Loyalist holdout, and exits through the intended grate/door. Every required staffing check has a single-player equivalent. Falling, missing required timing, and combat death still matter.

### 9.2 Recover the reactor contracts

Use group `0x80C43FF4` / key `0x686321C8` as the primary index.

- [ ] Map every platform's original path, order, transform, controller, and collision owner.
- [ ] Trace `of_platforms_held`, each `of_all_players` filter, and `pm_total_alive_count` to their actual inputs and consumers.
- [ ] Determine whether each native check means current occupancy, simultaneous occupancy, living participants, historical activation, or path completion.
- [ ] Recover start, warning, sink, reappearance, and reset controls with their native revisions and clock source.
- [ ] Recover `pm_encounter_start`, `pm_encounter_space`, and the four path-goal volumes, including shape and vertical bounds.
- [ ] Recover the klaxon/checkpoint light controls and what checkpoint advancement actually publishes.
- [ ] Recover anchor, dash, cluster, holdout, and holdout-door objective semantics. Names alone do not prove sequence.
- [ ] Recover source placements, selection lanes, tactical assignments, and wave cues for the 32 squads.
- [ ] Recover exit grate state and collision, completion audio, chest, death mechanic, and raid banner behavior if used.
- [ ] Classify unresolved host-only slots needed for these controls; leave unrelated holes unresolved.

Build a reactor sequence record from native references and player-flow evidence. Do not assume the four path counts determine branching, which platforms need multiple bodies, or the order in which all 32 squads appear.

### 9.3 Solo platform contract

Implement the smallest change that makes the recovered sequence possible for one real player:

1. Arm a path only after its actual entry conditions and required native objects are ready.
2. Accept activation only from the real player's valid interaction/occupancy on an eligible platform.
3. Retain that activation under the current attempt and current path.
4. Permit the next required step according to the recovered order.
5. Preserve native warning and sinking where compatible with the route; define physical hold behavior only where necessary for solo traversal.
6. Commit path completion only from the required activation set and the real path-goal observation.
7. Publish the verified checkpoint and prepare the next path.
8. On failure, reset the current path and its physical state without erasing earlier committed checkpoints.

Logical activation memory must not report that absent players are still standing on platforms. If a native device requires continuous occupancy to exist, discover a supported device hold/state input and scope its ownership to solo mode. A server latch cannot by itself keep a sinking physical platform walkable.

If branching requires revisiting a previous route, explicitly prove that a reachable path remains. Do not accept a solo rule that records progress correctly but leaves the player stranded. Determine route requirements from geometry before choosing hold durations.

Use a proposed state sequence such as idle, prepared, armed, activated, warning, sinking, reset, and committed only as a design vocabulary. Map each state to recovered native controls; do not assign arbitrary numeric device positions.

### 9.4 Incremental implementation order

- [ ] Publish the first path's required groups without triggering combat or later paths.
- [ ] Prove one platform: initial pose, collision, activation, warning, sinking, and reset.
- [ ] Prove two consecutive platforms with valid order, early departure, and wrong-order rejection.
- [ ] Implement the first complete path and its checkpoint.
- [ ] Repeat for paths two, three, and four, reusing the proved mechanism and each path's own geometry and identifiers.
- [ ] Add required path-specific combat in recovered order.
- [ ] Add checkpoint lights, warnings, encounter audio, and objective transitions.
- [ ] Add the Loyalist holdout with real population readiness and required death gates.
- [ ] Open the exit from the authentic holdout completion and verify its physical state.
- [ ] Add loot presentation/reward handling and reset according to their recovered contracts.

Do not wait until all four paths are scripted before trying the first platform in-game. Each accepted substep should become a permanent source implementation before expanding the path.

### 9.5 Holdout population and completion policy

For each cohort, specify source identities, selected actors, spawn cue, readiness requirement, required deaths, optional survivors, reinforcement cap, and retirement condition. Preserve sniper/anchor/fodder roles without automatically spawning all sides at full cooperative density.

Separate request acknowledgement from actor readiness and from death. A missing or delayed actor is not cleared. If a source contains multiple selected actors, its clear condition must account for that exact selection; a raw source count is insufficient.

Recover whether the encounter ends on a particular objective, final cohort, or combined required set. Do not create a room-wide kill requirement solely because the inventory contains additional squads. Stop reinforcement requests when the holdout completes and reject late requests from the old encounter owner.

### 9.6 Tests and gate G2

- [ ] All four paths are traversable by one player without debug movement or fake occupancy.
- [ ] Out-of-order, repeated, brief, and simultaneous-boundary occupancy are handled according to the selected policy.
- [ ] An activation from an earlier path/attempt cannot count for the current one.
- [ ] Native warning, sinking, collision, and reappearance agree with logical progress.
- [ ] Falling before and after a checkpoint produces the intended restart.
- [ ] A valid early goal crossing is retained only when the path's policy permits it; entering a volume alone cannot skip required actions.
- [ ] Lights and audio match the committed checkpoint and reset cleanly.
- [ ] Delayed enemy admission never invents death or blocks unrelated platform presentation.
- [ ] Required Loyalists can fight and die; required clear opens the exit once.
- [ ] Optional survivors do not leave an unrelated executor branch blocking completion.
- [ ] Exit collision, objective transition, reward presentation, and subsequent traversal work.
- [ ] Death during holdout restarts the holdout checkpoint without replaying all completed paths.
- [ ] A normal installed run from arrival through reactor exit succeeds without intervention.

**Gate G2:** the complete reactor/holdout section and its checkpoint failures are live accepted. No traversal or barrier gameplay implementation is required to declare this section accepted.

## 10. Milestone 3: mouth-to-belly traversal

### 10.1 Player-visible outcome

The player leaves the reactor, passes the false floor/crossing and airlock/piston environment, uses the native ejection sequence, and reaches the barrier arena. The intended hazards, checkpoints, dialogue, and optional ring reward behavior function.

### 10.2 Recovery tasks

- [ ] Trace the mouth phase tasks and distinguish similarly named crossing tasks by their source/bubble context.
- [ ] Recover the false-floor and crossing-door initial, triggered, open, and reset states.
- [ ] Recover each trigger's geometry, region, coordinate space, and once-per-visit semantics.
- [ ] Recover both airlock doors, interior/exterior sensors, exterior toggle, and mutual sequencing constraints.
- [ ] Recover piston animation, damage/blast timing, safe geometry, and reset behavior.
- [ ] Investigate the thunder-wall safe-zone owner and its root-specific activation threshold through `0x80F42E5C` and the linked owner config.
- [ ] Recover ejection-tube interaction, native launch/movement operation, wind volumes, destination, and completion observation.
- [ ] Recover seven ring objects and their individual activation/collection channels.
- [ ] Determine ring aggregation and whether it affects only a chest, a checkpoint, or required route progression.
- [ ] Recover the thundering-wall spawnpoint update and a safe receiving checkpoint.
- [ ] Identify the barrier engagement boundary without arming the barrier early.

The community piston cadence is a reference hypothesis. Do not use it as an installed constant. A platform animation and its damage pulse may have separate owners and clocks.

### 10.3 Implementation sequence

- [ ] Connect the authentic reactor exit to the mouth-crossing task.
- [ ] Preload the false floor and doors, then arm their proper triggers.
- [ ] Implement and test the airlock as a sequence with acknowledged door application and actual passage.
- [ ] Implement piston motion, hazard, safe zone, and local recovery.
- [ ] Implement the ejection interaction through its native owner; verify both departure and receiving location.
- [ ] Implement ring observations and their recovered aggregation separately from route completion.
- [ ] Implement the traversal chest if recovered; record actual reward support separately from its visible opening.
- [ ] Update the receiving checkpoint and “Delve deeper” presentation at the verified cue.
- [ ] Arrive at a stable barrier-ready state with no barrier targets or timer prematurely active.

Avoid replacing the ejection sequence with a debug teleport. If the final implementation needs a supported native teleport as part of the authored mechanism, record its exact place in that mechanism and verify its destination and control handoff.

### 10.4 Tests and gate G3

- [ ] Floor and doors show the correct initial geometry and collision.
- [ ] Early and repeated crossings cannot deadlock the airlock or replay a completed transition.
- [ ] Fast movement while dialogue is queued retains legitimate traversal observations.
- [ ] Piston motion, damage, and safety behavior are consistent with the selected clock and phase.
- [ ] Death near a door/piston restarts at a safe reachable checkpoint.
- [ ] Ejection works from the intended interaction and leaves normal player control at the destination.
- [ ] All ring objects are present; repeated activation cannot count twice.
- [ ] Missing a ring has the recovered consequence and cannot accidentally softlock required progression.
- [ ] Bonus rewards and already committed checkpoints do not duplicate after retry.
- [ ] Crossing the bubble boundary preserves intended globals and retires obsolete owners safely.
- [ ] A run from reactor exit to barrier readiness succeeds without intervention.

**Gate G3:** traversal, failures, and arrival at the barrier are live accepted. Begin cranium and barrier implementation here, in their first encounter context.

## 11. Milestone 4: Break the Barrier

### 11.1 Player-visible outcome

The player reads elemental targets, collects and charges the appropriate craniums, stages or carries them, destroys the required targets, handles adds, and completes the barrier encounter. Wrong choices and missed deadlines retain meaningful consequences. Completion reaches a stable Argos checkpoint.

### 11.2 Recover the reusable cranium system

Start with the barrier group's relic/fire records; do not infer their runtime meaning from repeated names alone.

- [ ] Map the 15 relic groups and their device/channel references to physical spawn points, variants, or state alternatives.
- [ ] Determine which references represent an uncharged pickup, carried weapon, placed charging object, charged pickup, and dropped item.
- [ ] Recover native pickup/use acceptance and transfer of ownership between world object and player.
- [ ] Recover insertion/placement at a fire station and the linked volume/interactable/channel.
- [ ] Map altar/cliffs/grove to Solar/Arc/Void from native fields or a verified live visual; record coordinate orientation.
- [ ] Recover charge start, clock, progress, completion, interruption, and retrieval conditions.
- [ ] Recover ammunition capacity, spending, matching-element damage, wrong-element behavior, and depletion.
- [ ] Recover drop/despawn timers and what native owner retains a staged item.
- [ ] Recover respawn/replenishment so losing or exhausting a cranium cannot leave an unwinnable attempt except through an explicit failure rule.
- [ ] Recover reset behavior for held, charging, charged, and dropped craniums.

Desired logical states are available, held uncharged, inserted/charging, charged available, held charged, dropped, depleted, and retired. Each transition needs a genuine interaction/state observation. A timer in Lua alone does not create a charged weapon or transfer ownership.

### 11.3 Prove cranium interactions before scheduling the full encounter

- [ ] One native pickup appears at its authored position and can be collected.
- [ ] The correct held object/weapon exists with a verified player owner.
- [ ] One station accepts an uncharged cranium and starts its actual charging behavior.
- [ ] Native charge completion creates/exposes the correct elemental pickup.
- [ ] Retrieval gives the intended elemental weapon and ammunition.
- [ ] Firing damages a matching native target and consumes ammunition.
- [ ] Wrong-element fire follows the intended rejection behavior.
- [ ] Dropping, re-picking, depleting, and losing the object have usable recovery paths.
- [ ] Reset removes the old attempt's held/world/charging ownership cleanly.
- [ ] Repeat for all three elements and multiple concurrent staged craniums.

Record visuals, sound, interaction prompts, charge progression, weapon behavior, and authentic damage separately. A correct inventory state without a usable beam is not acceptance.

### 11.4 Recover target, scheduling, and wipe controls

Use `0x80C43A1D` / key `0x91264981`:

- [ ] Map the six `oracle_spawner` structures, their top/middle/bottom triplets, geometry, gates, and sequences.
- [ ] Determine which target sets can be active together and how elements/positions are selected.
- [ ] Recover arming, vulnerability, destruction, despawn, and replacement of each selected target.
- [ ] Recover wipe timer start, cancellation, and actual failure action.
- [ ] Recover the condition for one set complete, one section complete, and the whole barrier complete.
- [ ] Recover the cooking-boss-shield device and the geometry/collision changes on completion.
- [ ] Recover each side's regular, cloud, and anchor squads and their independent scheduling cues.
- [ ] Recover source spawn regions, reinforcements, optional death policies, and terminal cleanup.
- [ ] Recover chest, raid death mechanic, and the handoff to the boss checkpoint.

Do not make destruction count alone authoritative if it can include stale, unrelated, or not-selected objects. Authenticate the current selected target set and encounter round.

### 11.5 Solo timing and pressure

Measure a real solo sequence before selecting deadlines:

1. Time reading the required elements and choosing a route.
2. Time pickup, travel, insertion, native charging, retrieval, and travel to firing position.
3. Time aiming and destroying each required target with the available ammunition.
4. Repeat for mixed elements, repeated elements, and the longest valid route.
5. Add the measured combat interruption allowance and tune a margin for ordinary mistakes.

Set a bounded staging lifetime and deadline that permit these actions while retaining urgency. Decide whether preparation is permitted before target activation based on the recovered trigger and selected solo policy. Explicitly define when each timer begins: native target armed, station accepted, charge completed, or item dropped.

Keep three concepts separate: native charge duration, dropped-item persistence, and encounter failure deadline. Extending one must not freeze the entire native gameplay clock or alter other activities.

Begin with reduced simultaneous side pressure and retain recognizable enemy roles. Stagger waves using the authentic encounter phase and native source availability. Do not compensate for an impossible travel deadline with invulnerability or fabricated progress.

### 11.6 Barrier implementation sequence

- [ ] Implement engagement and a stable pre-start staging state.
- [ ] Implement one target set with correct element selection and failure deadline.
- [ ] Implement its genuine destruction/completion and next-set scheduling.
- [ ] Add the remaining recovered set/section sequence with unique round ownership.
- [ ] Add side populations, reinforcement policy, and independent optional/required death rules.
- [ ] Add objective, markers, effects, music, and charge/target feedback.
- [ ] Implement barrier completion, retirement of active failure owners, and physical opening.
- [ ] Commit the Argos checkpoint only after the barrier's terminal state and a safe spawn are established.
- [ ] Integrate retry and end-to-end approach from traversal.

### 11.7 Tests and gate G4

- [ ] All three elements can be collected, charged, staged, carried, fired, and replenished.
- [ ] Mixed and repeated element combinations are possible for one player.
- [ ] Partial charge interruption and completed-charge persistence behave as specified.
- [ ] A staged cranium has one owner and cannot duplicate through pickup/drop races.
- [ ] Wrong element, insufficient ammunition, lost cranium, and missed deadline have defined consequences.
- [ ] Every selected target requires genuine matching damage/destruction evidence.
- [ ] Last-target destruction near the deadline resolves consistently using recorded event order/timestamps.
- [ ] A stale destruction or delayed wipe from an old round cannot affect the next round.
- [ ] Populations spawn with real AI and native ownership; optional survivors do not block genuine barrier completion.
- [ ] Timer cancellation removes the actual pending failure behavior, not just its UI.
- [ ] Barrier opening changes both presentation and collision correctly.
- [ ] Wipe resets targets, charging stations, held/dropped relics, enemies, timers, and objective state.
- [ ] Retry needs no reload or debug restoration.
- [ ] The full encounter works from the natural traversal entry in an installed candidate.

**Gate G4:** barrier completion and wipe recovery are live accepted. The cranium implementation is now the baseline for Argos; do not duplicate it with a separate unverified boss-only version.

## 12. Milestone 5: Argos

### 12.1 Player-visible outcome

The native Argos reveal leads into a complete solo boss fight: prepare elemental craniums, converge the shield targets, deal damage, handle attacks and detainment, interrupt the wipe attack, repeat as required, and produce an authentic boss death ready for the ending.

### 12.2 Recover boss ownership and presentation first

Use group `0x80C42FA3` / key `0xE8D290A0`:

- [ ] Resolve `boss_manager.sq_boss` and candidate `0x80C75F82` through source admission, entity, character, health, AI, tactical owner, and scene cast.
- [ ] Preserve the serialized rule sentinel where present; do not invent a rule registry because the separate rule reference is absent.
- [ ] Recover initial boss geometry, shield, immunity, pose, and collision states.
- [ ] Decode scene graph `0x80F44500` for the intro and `0x80F444FB` for alpha-strike facing: cast order, prerequisites, event inputs, outputs, and lifetime.
- [ ] Resolve reveal cinematic descriptor `0x8155C01E` at offset 744 and distinguish its ownership from the two scene descriptors.
- [ ] Recover when AI and attacks begin relative to the visible reveal.
- [ ] Recover confirmed scene application, animation, and completion observations; avoid accepting a request as playback.

Prove a visible, animated, correctly owned Argos before adding shield cycling. An admitted but invisible boss is not ready for combat acceptance.

### 12.3 Recover shields and target convergence

- [ ] Map the three shield sections, their state channels, missile managers, and related geometry/gates.
- [ ] Recover the active section/node selection and each required target's element and start position.
- [ ] Recover how matching cranium fire moves a target and how convergence is detected.
- [ ] Distinguish target movement, target completion, shield visual state, and actual damage immunity.
- [ ] Recover native target reset/return behavior and the owner that drives it.
- [ ] Identify a supported way to retain completed target position without disabling unrelated native updates.
- [ ] Recover transition to vulnerability and the condition that closes it.

### 12.4 Implement sequential solo convergence

The intended solo change is bounded retained progress:

1. The current shield set identifies the required targets and elements.
2. The player stages the appropriate charged craniums using the accepted barrier system.
3. Correct native beam interaction moves a target to its required convergence point.
4. Genuine arrival records that target as parked for the current shield set and starts its retention window.
5. The player switches craniums and repeats for the remaining targets.
6. When all required targets are genuinely parked within the permitted overlap, request the supported native shield-break transition.
7. Require native vulnerability application before treating normal weapon damage as enabled.
8. If the window expires, release retained ownership and reset the relevant set through the supported mechanism.

The native target must visibly stay parked. A hidden server completion flag with the target drifting away is insufficient. Do not repeatedly teleport a target or freeze its entire behavior graph to mask a missing hold contract.

Calibrate the retention window against actual drop/pickup, repositioning, and aiming. If the native target cannot be held through any recovered operation, record the constraint and design the smallest explicit solo adapter. Replacing the convergence puzzle with generic damage is a material change to discuss.

### 12.5 Damage, attacks, and immunity

- [ ] Recover the boss health/damage owner, shield immunity, buff/debuff channels, and their true consumers.
- [ ] Recover whether the damage window ends on a timer, scene event, channel transition, or a combination.
- [ ] Preserve authentic damage processing and boss death. A zero-health sample is not interchangeable with the confirmed native death event.
- [ ] Recover missile spawn/fail sequences and which projectiles create pressure versus wipe conditions.
- [ ] Recover Harpy swarms and side populations, with their selected source sets and retirement policy.
- [ ] Recover unstable-energy or section-specific damage effects without assuming their guide description proves exact native behavior.
- [ ] Validate boss visuals and damage immunity independently; hiding a shield mesh must not be the damage gate.
- [ ] Record any shared damage interception reused and ensure it scopes only to the admitted Argos owner and solo activity.

Balance health only after a full damage window works. Measure damage under representative ordinary loadouts, including a weak damage phase. Preserve required mechanics when heavy burst damage arrives; recover Argos-specific transition rules rather than copying Dendron's one-third thresholds.

### 12.6 Detainment and self-rescue

- [ ] Trace the production detain projectile, sphere/cage owner, target player, restrictions, damage-over-time, and release/retirement.
- [ ] Inspect `0x80C75E63` and `0x80F44464` through the verified native node mapping and their full linked object lifecycle.
- [ ] Determine whether the player retains aiming/firing and whether inside-origin damage can hit the cage.
- [ ] Recover the native damage/destruction event that releases the same captured player.
- [ ] Test cage destruction with correct owner, unrelated damage, expired timer, death, and scene transitions.

Preferred solo behavior is a short, readable opportunity to destroy the player's own cage with a native supported attack. If inside damage is blocked by collision or input restrictions, investigate a narrowly scoped solo escape interaction that still requires player action and a native release. Do not claim inside shooting works before a live test, and do not silently remove detainment or auto-free the player.

The cage must never leave the player permanently restricted after a failed attempt, boss death, or checkpoint reset. A delayed old-cage release cannot free or damage a new capture.

### 12.7 Wipe attack and weak points

- [ ] Trace the production alpha-strike owner and its relationship to scene facing, platforms, weak-point visibility, timer, and wipe damage.
- [ ] Distinguish production root `0x80F44153` from test/wipe roots `0x80F42E67` and `0x80F42E68` and identify which are actually reached in this encounter.
- [ ] Recover target selection, required simultaneous/consecutive destruction semantics, damage type requirements, and weak-point lifetime across cycles.
- [ ] Recover the real interrupt/stun output and cancellation of the pending wipe action.
- [ ] Recover failure completion and transition into checkpoint reset.

The guide's two-of-six weak-point rule is an acceptance hypothesis until confirmed. If confirmed, first preserve two required targets with solo-scaled health and a measured repositioning window. Do not reduce it to one solely because an earlier brainstorm suggested it.

After a valid interrupt, require the actual native stun/recovery and return to the next shield cycle. A server counter increment is not proof that the wipe owner stopped or that Argos returned to a playable state.

### 12.8 Repeated cycles and terminal priority

Define the cycle contract only after its native boundaries are recovered. A proposed sequence is shield preparation, convergence, vulnerability, post-damage transition, wipe windup, successful interrupt, recovery, and next shield set. Confirm actual order from references and native observations; the graph names are not evidence.

Each cycle needs a fresh cycle identity beneath the same boss incarnation and encounter attempt. Track selected target sets, retained convergence, damage-window ownership, cages, missiles, weak points, and failure timers. Distinguish logical cycle reset from physical boss recreation.

Establish event priority for boss death, player death, completed interrupt, shield completion, and timer expiry from native event order and documented solo policy. Process the accepted terminal event once. A scheduled next cycle must not resurrect the boss after genuine death, and an old wipe callback must not kill the player during loot presentation.

Use the supported repeat/composition mechanism established earlier. Boss policy can be authored data with native mechanics executing each cycle; do not assume a long-running Lua VM exists.

### 12.9 Incremental implementation order

- [ ] Boss admission, initial geometry, reveal, and stable shielded state.
- [ ] Accepted relic system in the boss arena, including staged pickups and reset.
- [ ] One shield target's correct movement and native return behavior.
- [ ] Solo parking of that target with bounded expiry and visible persistence.
- [ ] Full required convergence and acknowledged vulnerability.
- [ ] One genuine damage window and return to a stable next state.
- [ ] Missile/Harpy/side pressure under bounded population policy.
- [ ] Detainment with a proven solo escape and failure cleanup.
- [ ] Alpha strike, required weak points, genuine interruption, and actual wipe.
- [ ] Multiple complete cycles with correct state ownership.
- [ ] Genuine boss death, retirement of combat/failure owners, and ending handoff.
- [ ] Integrated playthrough from the barrier checkpoint and then from the raid entrance.

### 12.10 Tests and gate G5

- [ ] Intro/cast/animation occur once at the correct cue; retry behavior is explicitly selected and functional.
- [ ] Boss is immune before a valid shield break and damageable only during the supported window.
- [ ] Every required elemental combination can be prepared and converged solo.
- [ ] Partial convergence, wrong element, lost relic, expired hold, and repeated target events reset correctly.
- [ ] Visible shield and vulnerability agree; staged relics remain usable through allowed transitions.
- [ ] Low-damage, high-damage, and repeated cycles preserve required mechanics and native boss identity.
- [ ] Detainment can be escaped by the player and cleans up on failure, death, and terminal transitions.
- [ ] Weak-point success genuinely cancels the wipe and returns the encounter to a playable state.
- [ ] Weak-point failure performs the intended wipe and permits a clean retry.
- [ ] Late missiles, cage damage, old weak-point deaths, and old cycle timers cannot affect a new attempt or the ending.
- [ ] Authenticated boss death wins exactly once and prevents further attack/cycle requests.
- [ ] No unrelated optional population/readiness branch blocks the death handoff.
- [ ] The complete boss fight, including at least one deliberate failure and retry, works in an installed candidate without intervention.

**Gate G5:** Argos is solo playable and resettable through authentic death. The raid is not complete until the post-death ending also passes G6.

## 13. Milestone 6: ending and rewards

### 13.1 Recover the ending contract

- [ ] Follow genuine Argos death to the native death presentation, treasure-chamber task, teleporter destinations/returns, and final environment changes.
- [ ] Recover which remaining hazards, wipe actions, cages, buffs, and combat sources retire before player transport.
- [ ] Recover whether the intended transition is a native rescue/transport, scene, or a combination, and identify the receiving spawn/control handoff.
- [ ] Bind the defeated-boss dialogue and final safety/reward selectors to their correct story contexts and playback owners.
- [ ] Recover the chest's spawn/open interaction and the available reward/investment path.
- [ ] Recover final objective cleanup, mission-complete publication, and allowed player exit.

Do not copy Omega's Lighthouse destination or teardown order just because its transition already works. Reuse the lifecycle mechanism with Eater's own owners, transport, and presentation dependencies.

### 13.2 Implementation sequence

- [ ] Accept the authenticated death for the admitted boss/current attempt.
- [ ] Stop new combat requests and cancel/retire all active failure owners safely.
- [ ] Complete the required native death and ending presentation without premature source retirement.
- [ ] Request the recovered transport/scene when its prerequisites are satisfied.
- [ ] Observe real receiving arrival and restore normal player control.
- [ ] Publish the reward chest and preserve its intended interaction lifetime.
- [ ] Play the selected closing exchange once, with a documented completion observation.
- [ ] Clear/complete the objective and publish mission success through the shared lifecycle service.
- [ ] Permit the intended exit without cutting off required speech or cleanup.

Treat visible chest spawning, opening animation, reward grant, and mission success as separate features. If the local reward backend lacks a recovered grant path, record it explicitly and resolve the desired local reward policy; do not silently call a decorative chest a complete reward implementation.

### 13.3 Tests and gate G6

- [ ] Boss death consistently reaches the intended final area/presentation.
- [ ] No lingering immunity buff, restraint, projectile, damage-over-time, wipe timer, or hostile respawn affects the ending.
- [ ] Player arrival and control are valid; the reward chest is usable.
- [ ] Actual reward behavior matches the documented supported policy and cannot duplicate through repeated completion callbacks.
- [ ] Final speech is audible, ordered, and finishes before the required completion transition.
- [ ] Scene completion is distinguished from conversation completion if an idle scene remains active.
- [ ] Mission success publishes once and the player can leave normally.
- [ ] Exit/relaunch cannot reuse the previous attempt's rewards, owners, or completion state.

**Gate G6:** ending, reward policy, and mission completion are live accepted. Proceed to full-route acceptance with the same coherent candidate.

## 14. Lifecycle, checkpoints, and recovery

### 14.1 Identity hierarchy

Use the existing runtime's equivalent fields for these distinct scopes:

- Activity run: one launch/session.
- Encounter attempt: one try between checkpoint resets.
- Path/round/cycle: one reactor path, barrier set, or boss cycle.
- Source generation and salted native owner: one actual admitted object/actor incarnation.
- Command token/revision: one requested operation and its matching acknowledgement.

Do not assume all of these scopes can be represented by a single generation counter. Use existing identities where sufficient and extend only a missing scope. Every observation must be validated against the identity relevant to its contract.

A new command revision can leave the same boss alive. A new encounter attempt invalidates old shield targets and timers even if the activity run persists. A new source generation does not make an old captured pointer safe.

### 14.2 Checkpoint policy

Proposed single-session checkpoints, to bind to safe recovered spawn locations:

- Arrival/entrance.
- Each recovered reactor checkpoint; holdout as a separate restart where supported.
- Safe traversal checkpoints, including the ejection receiving point if supported.
- Barrier encounter start.
- Argos encounter start after committed barrier completion.
- Ending terminal state, with no combat restart after committed success.

For each checkpoint, record spawn identity/transform, active bubble/region, required resident groups, door/platform state, completed objective state, dialogue replay policy, player restrictions, and populations to create or retire.

Keep accepted encounter completion within the activity session. Persistence across closing the game or restarting the process is a separate feature: inspect current save/checkpoint infrastructure and record whether it is supported. Do not promise durable raid saves from an in-memory checkpoint ledger.

### 14.3 Reset sequence

1. Accept a genuine failure/player-death condition under the current attempt.
2. Stop scheduling new commands for that attempt and invalidate its pending progression.
3. Cancel/retire active wipe, charge, convergence, projectile, detainment, scene, and population owners according to their native lifetimes.
4. Wait for necessary native retirement/application acknowledgements; prevent two owners from controlling the same source.
5. Clear attempt-local logical state while preserving earlier committed encounter checkpoints and generation high-water marks.
6. Restore the checkpoint's exact object/door/platform/boss presentation and gameplay state with fresh revisions.
7. Publish the supported player respawn/placement and observe safe arrival/control.
8. Re-arm the encounter only after its required native readiness is re-established.

Determine how the runtime actually orders player death, respawn, retirement, and publication. The list above is the required dependency intent, not a substitute for that native contract. Do not hold a lock across native teardown or restore bytes into a reused allocation.

### 14.4 State that must be audited on every reset

- Platform activation memory, physical poses/collision, warning and sinking timers.
- Door/airlock/piston phase and checkpoint lights.
- Ring collection and bonus chest policy.
- Craniums held by the player, inserted, charging, staged, dropped, or depleted.
- Target selection, damage/destruction state, barrier set progress, and failure deadlines.
- Boss health/pose/immunity, shield set, parked targets, damage window, weak points, and cycle counters.
- Detainment restrictions, cage owners, missiles, damage-over-time, temporary buffs, and wipe actions.
- Initial and reinforcement populations, pending admission, optional survivors, and owned death observations.
- Dialogue queue, scene inputs/cast, objective/marker state, music, and completion latches.
- Runtime capacity reservations, publication revisions, command tokens, and diagnostic timers.

### 14.5 Boundary cases

Test failure during charge, during convergence, at target deadline, in the boss intro, while detained, during alpha strike, immediately after native boss death, and during transport. Specify a deterministic result for each.

Differentiate local retry, bubble unload/reload, return from another area, return to activity selection, and a fresh process. Passing one is not evidence for the others.

## 15. Population and solo balance

### 15.1 Per-cohort population record

For every requested cohort, record:

- Native registry/source/type/slot and tactical row.
- Selected category/lane and actual actor count.
- Exact placement and any permitted variation.
- Request trigger and whether a native-ready wait is required before proceeding.
- Required health, AI, and tactical ownership observations.
- Required death set and optional survivor policy.
- Reinforcement condition, delay origin, active cap, and maximum outstanding requests.
- Cleanup behavior on success, wipe, phase transition, unload, and late admission.

Use the recovered formations and roles. Test behavior as well as existence: navigation, aiming, attacking, shielded states, and deaths. Never count retirement, absence, allocation failure, or a zero sample as a genuine kill.

### 15.2 Capacity policy

Keep admission bounded by the actual native capacity and the shared service's supported behavior. A temporarily full pool should queue/defer according to a defined policy, not silently drop a required source or raise the global actor limit.

Prioritize required mechanic actors before optional pressure when the existing admission contract supports priorities. If it does not, schedule within known budgets. Capacity pressure must never leave a mandatory cranium, boss, target, or exit device permanently unavailable.

Include transition overlap in the budget: objects can remain visible while the next scene/cast or area begins loading. Observe actual retirement before reusing ownership or claiming capacity has returned.

### 15.3 Balance method

Begin with one solo preset. Preserve original identities and roles, then adjust:

- Maximum simultaneous adds and reinforcement frequency.
- Mechanic target and boss health through supported, scoped controls.
- Cranium staging lifetimes and encounter deadlines.
- Convergence retention and weak-point repositioning time.
- Resource availability, replenishment, and checkpoint recovery.

Tune one variable family at a time, after mechanics work. Record a representative loadout, elapsed time, damage per window, deaths, interruption time, and the reason for each change. A three-phase kill or a 35–45-minute raid is not a requirement established by evidence; earlier conversational estimates must not become a hidden completion target.

Keep enemy AI and recognizable attacks intact. Do not compensate for unresolved mechanic bugs with infinite ammunition, player invulnerability, skipped targets, universal one-hit enemies, or a silent disabled wipe.

## 16. Objectives, dialogue, scenes, and music

### 16.1 Objective identities

Bind the seven installed objective events from table `0x80C43A4C`:

- `0x54DECB72`: Explore the Leviathan.
- `0x70E54B0C`: Explore the Leviathan, separate event with matching text.
- `0x440F76C5`: Escape the reactor.
- `0x7F4355DF`: Defeat the Loyalists.
- `0xAF9A3F59`: Delve deeper.
- `0x6BC38FD1`: Break the barrier.
- `0x4E2D6094`: Destroy Argos.

Resolve the trigger for each event and marker separately. Serialized row order is useful evidence but does not establish every transition. Preserve the two Explore rows and determine their contexts instead of deduplicating by text.

### 16.2 Dialogue selectors

Bank `0x80F1F9A5` has six recovered selectors:

- `0x0B6F9EC9`: opening/engine problem variants; recorded duration 14,656 ms.
- `0xE00FB2F6`: deeper invitation variants; 8,665 ms.
- `0xDA634408`: welcome/return variants; 13,281 ms.
- `0x2A61BC44`: defeated Argos/engine restored variants; 9,957 ms.
- `0x55C33B57`: safety/reward variants; 38,476 ms.
- `0x74F16395`: Loyalists/repeat-run variants; 11,628 ms.

These are recovered metadata and content groupings. They do not prove a script order, each alternative's actual spoken duration, or who submits the line. Resolve selectors by their IDs, not guessed array positions. Do not concatenate alternative branches into a single conversation.

For each exchange, identify one playback owner: native scene speech or the shared dialogue service. Record queue acceptance, actual submission/start, and completion separately. If completion uses an acknowledged bank duration rather than a speech-finish observation, document that limitation and verify the selected branch audibly.

### 16.3 Scene and timing rules

- Recover the complete cast, including markers and non-actor references.
- Keep scene-owned sources under their native owner and avoid independently spawning a duplicate.
- Preserve legitimate early approach signals while waiting for other prerequisites.
- Start relative timing from the relevant authenticated native event.
- Keep animation progression independent of an unrelated queued line unless the recovered scene requires it.
- Distinguish scene finish from speech finish when a scene retains an idle branch.
- Keep visible scenery alive until its intended presentation finishes, then retire under the correct owner.
- Test scene behavior when the player moves quickly, pauses, dies, leaves/re-enters, and retries.

For music and effects, recover phase-linked sequences and controls where possible. Label any reconstructed score selection or cadence. A repeated hash/name search alone does not prove the music scheduler.

### 16.4 Player-facing guidance

Use the original objective text and native cues wherever they communicate the solo rule adequately. Add concise solo guidance only where a changed rule would otherwise be unclear, such as retained shield targets or a new supported cage escape interaction.

Keep native hashes, binding names, debug counters, and technical status out of the ordinary player flow. Put diagnostics in the developer log or explicitly enabled debug view.

## 17. Validation, packaging, and delivery

### 17.1 Distinct acceptance levels

Use separate statuses rather than a single ambiguous “done”:

1. **Recovered:** identity and contract have evidence; no implementation implied.
2. **Implemented:** code/script is connected to the active runtime path.
3. **Offline validated:** relevant fixtures, tests, and builds pass for exact source.
4. **Installed:** matching payload is present, with backup and receipt.
5. **Live accepted:** the specific mechanic/section works in the recorded run.
6. **Integrated accepted:** the natural route through the section works without intervention.

A build with a simulated full-route test is still only offline validated until the native game completes the route. A temporarily repaired live run proves the intervention's result; the permanent source fix requires its own fresh candidate acceptance.

### 17.2 Meaningful offline tests

Add tests for recovered contracts and real failure risks, not tests that merely reproduce implementation formulas:

- Independent package fixture validates group identity, cross-package provenance, original slot holes, and descriptor classes.
- Launch fixture distinguishes activity identity, local bubble ordinal, map-global index, region, and spawn set.
- Device/authority tests use independently known nonzero revisions, modes, bit alignment, native units, and original consumer behavior where feasible.
- Ownership tests reject wrong source, stale attempt, wrong cycle, retired controller, and duplicate event.
- Reactor tests cover order, activation retention, physical-state acknowledgements, path commit, and checkpoint reset.
- Traversal tests cover early/repeated volumes, door ordering, receiving arrival, and optional ring results.
- Relic tests cover pickup/insert/charge/drop/depletion ownership, interrupted charge, completed persistence, and reset.
- Barrier tests cover target-set selection, authentic destruction, deadline ordering, repeated sets, and real failure cancellation.
- Argos tests cover convergence overlap, true vulnerability, repeated cycles, detainment ownership, interrupt/failure, and terminal priority.
- Lifecycle tests cover delayed admissions and callbacks after success/reset, preserved completed encounters, and source retirement.
- Lua validation rejects unsupported capabilities, invalid arguments, cycles, oversized graphs, and ambiguous phase wiring.

Register each new suite in the current runner and use only configurations its project supports. Existing native suites may be Release-only. Do not fabricate missing capture inputs, mark compile-only results as executed, or bypass a validation gate because an old fixture is unavailable; report its actual coverage and use independent available evidence.

### 17.3 Live playtest protocol

For each section candidate:

1. Install the coherent validated DLL/script set once the game is closed.
2. Start a fresh process and verify the mission selection, script fingerprint, and expected build.
3. Enter through the latest accepted natural predecessor or a clearly labeled development checkpoint.
4. Test one uncertain mechanic at a time and capture actual result and ownership.
5. Test the section's normal completion and its deliberate failure/retry cases.
6. Carry any successful live diagnosis into source and rebuild; do not leave the accepted result dependent on a temporary hold.
7. Run the section naturally from its predecessor without interventions before accepting its gate.

Development checkpoints are useful to repeat a local test. They must restore the complete native checkpoint contract and be clearly distinguished from final acceptance. Reaching Argos by a debug jump cannot prove arrival, reactor, traversal, or barrier progression.

No UI-driving automation is assumed by this plan. Use the actual supported tools and the user's current live-test authorization. If player positioning or a visible action is needed, request that concrete action when needed rather than asking broad repeated permission questions.

### 17.4 Candidate build and package procedure

Read current tool arguments and manifests before execution. After adding Eater's script, tests, and integration to those tools, the existing workflow is:

```powershell
python tools/coo/verify_lua.py --out build/coo/eater-<milestone>-<candidate>
python tools/coo/package_lua.py --validation build/coo/eater-<milestone>-<candidate>
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/eater-<milestone>-<candidate> -ValidateOnly
```

After the game is closed and installation is within the authorized implementation scope:

```powershell
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/eater-<milestone>-<candidate>
```

These are future execution templates, not commands run while creating this plan. Replace placeholders with a unique directory and preserve existing candidate evidence. Focused tests are appropriate during development; the full current configured validation is required by the full package path. A separate Eater-only package scope would need an explicit tested implementation in the tooling.

Freeze source before final validation/package, including any documentation inside the hashed manifest. Detect concurrent edits and rebuild the affected candidate rather than mixing validated binaries with later scripts. Preserve matching PDBs, exact payload hashes, and the prior coherent DLL/script set. A backup of already edited scripts alone may not match the previous installed DLL.

Lua changes apply on a fresh process under the current loader. A compiled binding change also requires a rebuilt DLL. Do not claim normal hot reload because a one-off live diagnostic patch was possible.

### 17.5 Full-route acceptance gate G7

The final candidate must pass all of the following:

- [ ] Fresh launch from the normal solo Eater selection.
- [ ] Arrival, control, entrance, and opening presentation.
- [ ] All four reactor paths, correct checkpoints, and Loyalist holdout.
- [ ] Intended traversal hazards, ejection, receiving arrival, and correct ring/reward behavior.
- [ ] Complete barrier encounter with all required elemental interactions.
- [ ] Native Argos intro and a complete solo boss fight through genuine death.
- [ ] Ending transport/presentation, documented reward behavior, final speech, and mission success.
- [ ] A second fresh launch does not inherit previous completion or duplicate one-time ownership.
- [ ] Deliberate failure/retry has been accepted in reactor, barrier, and Argos on this candidate or a demonstrably unchanged equivalent mechanic build.
- [ ] No debug skip, manual completion, held memory value, synthetic death, or temporary intervention is needed.
- [ ] No unresolved blocker prevents required normal behavior; all optional differences and reconstruction choices are documented.
- [ ] Affected existing mission regressions and current package requirements pass.

If code changes after full-route acceptance, assess affected coverage. Repeat the full route when the change affects shared lifecycle, routing, protocol, or connected progression; otherwise repeat the meaningful affected segment and record why remaining evidence still applies. Avoid repeatedly running unrelated suites without a new change or concern.

### 17.6 Final delivery contents

- Updated source and `eater_of_worlds.lua` with the actual implemented capabilities.
- Reproducible catalog/binding extracts and source provenance.
- Current solo rules with selected values and measured rationale.
- Implementation and acceptance records showing the exact candidate and tested route.
- Full configured validation results and known unavailable coverage.
- Coherent package, installed manifest, matching symbols, and rollback location.
- Launch instructions and checkpoint/retry behavior understandable to the player.
- Explicit remaining fidelity differences, optional features, and unaccepted cases.

## 18. Troubleshooting order

### 18.1 When an object is missing or inactive

Check in order:

1. Correct activity/scenario and current region/bubble/slice.
2. Required package and registry group residency.
3. Exact descriptor identity and original slot layout.
4. Published request and native source/entity/controller creation.
5. Current ownership/generation and expected runtime class.
6. Desired native state and acknowledged revision.
7. Pending native target/animation that may undo the current pose.
8. Model, collision, effect, and gameplay state separately.

Do not start by changing an arbitrary device value, forcing all groups resident, or bypassing readiness. Beyond's definition-versus-runtime class mismatch and repeated reflected controller rows show why exact identity matters.

### 18.2 When progress freezes

- Identify the specific active command and missing required observation.
- Determine whether it is a request, creation, application, interaction, animation, damage, death, or dialogue wait.
- Verify the authentic trigger and current-attempt observation are present.
- Audit all pending graph branches, including optional enemy waits unrelated to the final action.
- Verify the native gameplay clock advances and uses the right units/epoch/encoding before changing timers.
- Check whether reset/unload or a scene handoff changed the owner.
- Check bounded capacity and queued admissions before increasing any limit.

Never release a stuck graph with fabricated readiness, destruction, or death. Fix the incorrect binding, ownership, observation, or authored dependency.

### 18.3 When a boss animation or scene fails

- Confirm the actual compiled owner is the one being changed.
- Verify the admitted actor and full ordered scene cast.
- Check native event prerequisites, current queue/program revision, and application acknowledgement.
- Trace child/parent handoff and actor lifetime; speech alone does not prove the actor remained bound.
- Determine whether an idle scene persists after the required speech/animation.
- Verify cancellation and replay belong to the current attempt.

Reusing Panoptes or Dendron's service is appropriate. Copying their actor hashes, scene events, device polarities, or hard-coded phase thresholds is not evidence for Argos.

### 18.4 When a disconnect or crash occurs

Preserve the full log and candidate identity before rotation. Inspect earlier protocol, clock, allocation, publication, and ownership failures rather than blaming the last visible action. Beyond's clock byte-order problem produced a failure near combat progression while the cause was the timestamp encoding.

For a new native field or call, verify the image, ABI, thread, original-call forwarding, units, byte order, bit offsets, bounds, and owner lifetime. Independent original-consumer checks are stronger than a serializer and decoder that share the same mistaken assumption.

### 18.5 Live experiment record

Record the hypothesis, expected visible result, game/DLL identity, PID and process creation time, current run/attempt, exact native owner, smallest supported intervention, original state where relevant, native output, player observation, cleanup, and permanent source fix.

Discard live addresses after restart, unload, retirement, or owner change. Restore state only while the original owner is still valid. A successful temporary patch is evidence to explain and implement, not a reason to keep undocumented runtime forcing in the final raid.

## 19. Gap register

All entries below begin **OPEN** unless a later evidence record closes them. A material blocker stops acceptance of its own milestone, not unrelated recovery within that milestone. Close a gap with an evidence reference and result, not an optimistic note.

### 19.1 Foundation and arrival

- **EOW-01 — Activity variant and selection.** Resolve the suitable native row and selection route; preserve unknown retail labels. Close before G1.
- **EOW-02 — Arrival/spawn.** Resolve spawn set, transform, region, slice, local/global bubble mapping, and player-control handoff. Close with natural launch before G1.
- **EOW-03 — Cross-package catalog.** Publish required `0x0221` groups through verified provenance with intact slots. Close for each newly used group before its milestone's live test.
- **EOW-04 — Host-only declarations.** Classify the host-only slots required by the current mechanic; implement supported host policy where original scheduling is absent. Do not require decoding unrelated holes to finish arrival.
- **EOW-05 — Capacity and encoding.** Prove group/slot/actor/publication budgets and nonzero field correctness. Resolve before publishing a group or new authority field that crosses existing assumptions.
- **EOW-06 — Loader and delivery registration.** Register Eater consistently across selection, runtime, scripts, tests, packaging, and rollback. Close before the first installable candidate.

### 19.2 Reactor and traversal

- **EOW-07 — Platform state/occupancy.** Recover exact native modes, filter semantics, order, clocks, collision, and reset. Close through G2.
- **EOW-08 — Solo platform persistence.** Prove both logical progress and a reachable physical route. Close with solo path and failure tests at G2.
- **EOW-09 — Holdout scheduling.** Recover source selections and required clear; label authored wave policy. Close at G2.
- **EOW-10 — Checkpoint controls.** Recover safe spawns and state restoration at each section. Close incrementally and verify again at G7.
- **EOW-11 — Traversal triggers/devices.** Recover floor, doors, airlock, piston, launch, and receiving observations. Close at G3.
- **EOW-12 — Ring and chest semantics.** Establish required versus optional aggregation and actual reward behavior. Close the chosen supported behavior at G3.
- **EOW-13 — Thunder-wall lifecycle.** Recover the concrete owner threshold and relationship to safety/checkpoint behavior. Close when used by G3.

### 19.3 Barrier

- **EOW-14 — Relic ownership.** Prove pickup, insertion, charging, retrieval, drop, depletion, and cleanup. Close at G4.
- **EOW-15 — Element mapping/damage.** Prove station-to-element mapping, ammunition, damage, and rejection. Close for all elements at G4.
- **EOW-16 — Oracle selection and deadlines.** Recover selected sets, destruction, scheduling, wipe origin/cancellation, and completion. Close at G4.
- **EOW-17 — Solo barrier feasibility.** Measure valid routes, repeated elements, staging lifetime, and add pressure. Close with normal and failed attempts at G4.

### 19.4 Argos and ending

- **EOW-18 — Boss scenes/cast.** Recover intro, facing, reveal, event prerequisites, and complete native ownership. Close during G5 work.
- **EOW-19 — Shield convergence/authority.** Prove native movement, parked persistence, overlap, vulnerability, and reset. Close at G5.
- **EOW-20 — Detainment self-rescue.** Prove a usable player action and authentic release. A nonfunctional self-rescue blocks solo boss acceptance.
- **EOW-21 — Alpha strike.** Recover production owner, target selection, interrupt, wipe, and cleanup. Close at G5.
- **EOW-22 — Repeated cycle ownership.** Establish supported execution/composition, current-cycle observations, cancellation, and death priority. Close at G5.
- **EOW-23 — Ending transport/rewards.** Recover death-to-ending handoff, receiving control, reward path, final speech, and success. Close at G6.
- **EOW-24 — Full lifecycle acceptance.** Prove retries, unload/re-entry scope, terminal cleanup, and fresh relaunch. Close at G7.

### 19.5 Presentation across all stages

- **EOW-25 — Dialogue and objective contexts.** Bind each used selector/event to its actual cue and owner, including repeat variants. Close per section.
- **EOW-26 — Music and optional fidelity.** Recover or explicitly document reconstructed presentation scheduling. Do not let an optional score variation hide a required missing mechanic.
- **EOW-27 — Final tuning.** Select solo parameters from measured play, preserve failure pressure, and document remaining fidelity differences. Close for the delivered preset at G7.

## 20. Work packages and handoff

### 20.1 Ordered work packages

**WP0: establish baseline.** Finish G0, preserve research inputs, inspect the active runtime and current delivery pipeline.

**WP1: launch and entrance.** Resolve activity/spawn, publish needed groups, implement arrival presentation and entrance, then pass G1.

**WP2A: first reactor path.** Recover one platform end-to-end, then two-platform order, the first full path, and checkpoint reset.

**WP2B: remaining paths.** Extend with each path's own geometry, checkpoint behavior, and recovered combat. Verify no solo route becomes stranded.

**WP2C: holdout and exit.** Add selected populations, authentic clear, exit collision, and presentation; pass G2 through a natural run.

**WP3: traversal.** Implement floor/doors/airlock/piston, ejection, rings, receiving checkpoint, and barrier-ready arrival; pass G3.

**WP4A: cranium proof.** Complete pickup through matching-target damage for each element and clean reset, inside the barrier section.

**WP4B: full barrier.** Add rounds, deadlines, populations, solo staging, completion, and Argos checkpoint; pass G4.

**WP5A: Argos introduction and shield.** Recover cast/intro, reuse relics, prove target movement and solo convergence, and open a genuine damage window.

**WP5B: attacks and failure mechanics.** Add missile/swarm pressure, self-rescue, alpha strike, real interruption/wipe, and repeated cycles.

**WP5C: boss completion.** Accept full solo combat, death, and retry; pass G5.

**WP6: ending.** Complete transport, reward policy, dialogue, success, and cleanup; pass G6.

**WP7: integrated delivery.** Freeze the candidate, run required validations, accept the full natural route and fresh relaunch, and deliver G7 evidence.

No work package is assigned a fixed duration before its unknown native contracts are investigated. Track progress by closed bindings and accepted behavior. If a later research lead appears, record it for its package; continue the current encounter's implementation order.

### 20.2 Milestone closeout record

At the end of each work package, record:

- Current milestone and gate status.
- Player-visible behavior now working.
- New recovered identities and native contracts, with evidence.
- Source/script changes and any shared service extensions.
- Exact solo choices and selected values, with measurement where applicable.
- Focused/full test results and actual coverage limits.
- Candidate, installed payload, matching source/PDB, and rollback paths.
- Live run identity, tested cases, player observations, and interventions if any.
- Remaining blockers, next smallest experiment, and precise next work item.
- Unrelated working changes that must be preserved.

An idle handoff should make it possible for the next implementation session to continue without rediscovering the current state or repeating already accepted live tests.

### 20.3 First implementation session checklist

- [ ] Read this plan and current Eater evidence; inspect any changes since this document's date.
- [ ] Record current source/install baseline and active shared-file work.
- [ ] Reproduce or validate the Eater inventory against the current cache/packages.
- [ ] Trace the launch and missing spawn binding.
- [ ] Verify the entrance/shared groups and cross-package catalog contract.
- [ ] Implement only the minimal arrival/entrance slice and delivery registration.
- [ ] Run the appropriate focused checks and prepare a coherent candidate.
- [ ] Verify arrival in-game and fix the source until G1 passes.
- [ ] Continue with one reactor platform, not Argos or the full raid script.

### 20.4 Definition of complete

The task is complete when one real player can select solo Eater, traverse every required section, complete barrier and Argos mechanics, fail and retry the required encounters, receive the documented ending/reward behavior, and finish the activity in a coherent installed build without developer intervention. Required tests, source provenance, checkpoint behavior, and reconstruction differences must be documented.

A complete inventory, a compiled Lua script, a populated arena, a successful debug boss kill, or a test suite alone does not meet that definition.

## 21. Reference index

### Eater evidence

- [Research overview](README.md).
- [Original authoring handoff](AUTHORING-PLAN.md).
- [Native package and group map](NATIVE-PACKAGE-MAP.md).
- [Encounter mechanics and evidence limits](ENCOUNTER-MECHANICS.md).
- [Verified behavior roots and interpreter boundary](BEHAVIOR-EVIDENCE.md).
- [Source and reference provenance](SOURCES.md).
- [Machine-readable native inventory](evidence/native-inventory.json).
- [Extractor instructions](tools/README.md) and [extractor](tools/extract_eater_of_worlds.py).

### Current Sunrise authoring and execution

- [Mission implementation template](../../../Sunrise/docs/MISSION-IMPLEMENTATION-TEMPLATE.md).
- [Lua authoring and current limits](../../../Sunrise/docs/LUA-MISSION-AUTHORING.md).
- [New-mission reconstruction guide](../../../Sunrise/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md).
- [Shared mission services](../../../Sunrise/docs/UNIVERSAL-MISSION-SERVICES.md).
- [Validation runner](../../../tools/coo/verify_lua.py).
- [Package builder](../../../tools/coo/package_lua.py).
- [Candidate installer](../../../tools/coo/install_candidate.ps1).

### Prior boss and mission lessons

- [Garden World/Dendron implementation](../../../Sunrise/docs/GARDEN-WORLD-IMPLEMENTATION.md): native boss cycles, shields, genuine death, and cleanup.
- [Active Omega arm/animation/movement migration](../../../Sunrise/docs/NATIVE-OMEGA-ARM-MIGRATION.md): compiled-path verification, native control, and unresolved authority boundaries.
- [Omega ending teardown](../../../Sunrise/docs/OMEGA-ENDING-TEARDOWN-TO-CUTSCENE.md): explicit retirement and observed arrival/presentation.
- [Beyond Infinity implementation](../../../Sunrise/docs/BEYOND-INFINITY-IMPLEMENTATION.md): class/owner mistakes, device modes, native clocks, byte order, and the difference between live repair and installed acceptance.
- [Deep Storage implementation](../../../Sunrise/docs/DEEP-STORAGE-IMPLEMENTATION.md): separate preload/arming/death contracts, independent waves, optional survivors, and persistent completion.

Use these as implementation evidence and examples. Verify current source and installed receipts before treating a historical document's claims, paths, limits, or test counts as current runtime facts.
