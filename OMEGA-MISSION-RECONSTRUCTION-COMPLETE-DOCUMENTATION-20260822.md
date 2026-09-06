# Omega / mission_scot reconstruction: complete technical record

Date: 2026-08-22  
Project: Sunrise offline Destiny 2 activity reconstruction  
Activity: mission_scot  
Destination: Mercury, Lighthouse / Infinite Forest entrance  
Status: opening mission flow substantially reconstructed; Scene 2 is retained and Scene 3 proceeds; the closed entrance wall is still absent; the purple beam/aura remains spatially detached from the visible Ikora.

## 1. Purpose of this document

This is the canonical technical record for the Omega opening investigation. It consolidates the scene work, mission scripting, roster and authority reconstruction, schema recovery, VFX tracing, actor lifecycle work, closed-wall discovery, failed experiments, current implementation state, and the general rules that can be reused on other missions.

The older file OMEGA-PURPLE-VFX-HANDOFF-20260822.md remains useful as a detailed historical snapshot of the earlier VFX phase. This document supersedes it for current status because substantial work happened afterward:

- the three Ikora cast scenes were isolated experimentally;
- Scene 2's unwanted body and head were suppressed without removing its events;
- Scene 2's terminal retirement was suppressed so its state persists while Scene 3 still runs;
- the exact destination-owned closed wall and controller were identified;
- the foreign-registry packet route was tested and conclusively localized as a runtime-registration failure;
- the final low-level VFX transform experiment proved that even a successful write at the effect composer does not move the visible composite.

This document records discoveries, not just the latest code. Some experiments are intentionally disabled in current source because they were disproven.

## 2. Evidence vocabulary

Every strong claim in this document uses one of these meanings:

- **Confirmed:** directly observed in logs, runtime memory, package data, decompilation, or a controlled visual A/B test.
- **Strong inference:** several independent observations agree, but one semantic link is not directly named by the engine.
- **Hypothesis:** plausible and testable, but not yet demonstrated.
- **Disproven:** a guarded mutation or isolation test applied successfully and did not produce the predicted result.
- **Untested:** statically plausible but has not been run.

Pointers such as 000001EC... and object-table handles such as 17FAA001 are run-local. They must never be treated as durable identities. Definition tags, registry keys, scene handles, schemas, slot type/index pairs, generation, and timestamps are the durable evidence.

## 3. Executive summary

### 3.1 What now works

- mission_scot launches into the correct Lighthouse bubble and authored spawn context.
- The activity roster, player participation, lifetime, host readiness, activity script, mission director, and opening sensors initialize far enough for the native mission opening to execute.
- Ikora's dialogue and real authored animation play.
- All three native one-member Ikora scene casts can run in their normal order.
- Scene 1 and Scene 3 produce the correct non-VFX presentation.
- Scene 2's purple aura and beams execute from native authored events.
- Scene 2's unwanted visible body, cloth/mesh, and separate head are now suppressed at their exact model-construction callbacks.
- Scene 2's terminal actor-retirement transition is skipped, allowing its VFX state to persist indefinitely.
- Scene 3 remains enabled and native. Mission event completion still advances.
- A deferred host completion latch replaces the allocator-lifecycle signal intentionally skipped by infinite Scene 2 retention.
- The open Infinite Forest portal backdrop/state is visible.
- The deployed DLL and Release build are byte-for-byte identical.

### 3.2 What remains wrong

- The purple aura and beams are detached from the visible animated Ikora. They originate near the shared authored scene root rather than following the visible body's intended live pose.
- The retail closed triangular Vex wall is absent when the player first spawns.
- The attempted destination-registry wall objects are named and encoded correctly, but they do not exist in mission_scot's active authority container, so the client has nothing to apply the body to.
- Actual mission progression beyond the opening has not yet been completed or systematically audited.
- A +10 Z mutation remains active on four exact low-level purple effect handles. The write applies and reads back, but the user observed no visual movement. It is diagnostic residue, not a fix.

### 3.3 The most important conclusion

There are two separate unresolved presentation problems:

1. **Purple VFX binding:** the authored effect exists and runs, but its visible composite does not follow the desired Ikora pose.
2. **Closed wall materialization:** the exact wall asset and wire object are known, but mission_scot never instantiates the destination-owned registry entries that would receive the authority state.

Neither problem is caused by a missing scene event or a malformed wall packet.

## 4. Retail behavior being reconstructed

The intended opening is:

1. The player arrives at the Lighthouse with a closed, bright triangular Vex lattice blocking the Infinite Forest entrance.
2. Ikora speaks and begins her authored animation.
3. Purple Void aura and beams appear around/from Ikora.
4. The beams dismantle or transition the closed wall.
5. The open Infinite Forest portal backdrop becomes visible.
6. Ikora and the final portal state remain coherent while the mission script advances.
7. The player can continue through the opened entrance and proceed with the mission.

The current build starts much closer to step 5 than retail: the open portal is present, but the closed wall never materializes. The purple sequence plays, but its spatial anchor remains wrong.

## 5. End-to-end architecture

The reconstructed path is:

~~~
forced destination selection
  -> mission_scot launch descriptor
  -> activity message 5 phase 1: register roster groups
  -> bubble grant and player participation
  -> activity message 5 phase 2: instantiate/apply auth bodies
  -> host-ready latch
  -> activity script state 1
  -> complete authored sensor seed
  -> inactive Scene authority record resolves
  -> targeted Scene packet
  -> Ready packet
  -> player enters D00142CF/type-30/index-20 volume
  -> activity script state 2
  -> native opening timeline and one-member scene casts
  -> authored type-23 purple events
  -> D00142CF/type-43/index-1 handoff phase
  -> native cinematic retirement edge
       or Scene-2-retention substitute latch
  -> portal/gate/monitor authority delta
  -> activity script state 3
  -> settled keepalive state
~~~

This flow crosses several independent systems:

- destination selection and launch;
- membership and roster replication;
- package-derived scenario discovery;
- client runtime authority registration;
- auth-body decoding;
- mission script/director state;
- sensor input from the client;
- native scene scheduler and cast construction;
- model and renderer construction;
- authored event and VFX transform resolution;
- actor retirement and host-side completion.

A success at one layer is not proof that the next layer exists. This distinction explains both the VFX and wall failures.

## 6. Core destination and content identity

### 6.1 Launch identity

- Activity package/name: mission_scot
- Activity index observed in launch logs: 299
- Bubble: 15
- Bubble hash/name context: 0xA83A9175, Lighthouse
- Opening region/slice set: 120
- Opening state: 0 initially
- Spawn set hash used by the launch: 0x4AB3287A
- Absent/unset hash sentinel: 0x811C9DC5

The live settings file still displays its generic default destination at activity index 20. mission_scot is selected by the forced-destination path, not by replacing that generic JSON default.

### 6.2 Omega opening objects

- Complete opening registry object: 0x80F47BB2
- Primary mission opening registry key: 0xD00142CF
- Trigger volume definition: 0x80F47B6C
- Squad/spawner definition: 0x80F47B70
- Scene authority object: 0x80F47B73
- Gate controller: 0x80F47BA0
- Engagement sensor: 0x80F47BA3
- Vignette point: 0x80F47BA6
- Dialogue point: 0x80F47BA9
- Ikora entity definition: 0x80EC0F27
- Ikora presentation component: 0x80EC13A2

### 6.3 Scene and VFX identities

- Opening master timeline containing the purple type-23 events: 0x80FCCE87
- User-facing Scene 1 cast handle: 0x80EC0F0E
- User-facing Scene 2 cast handle: 0x80EC0FA8
- User-facing Scene 3 cast handle: 0x80EC0FA6
- Separate open portal scene: 0x80C51CBE
- Open portal scene reference observed earlier: 0x80C51CBD
- Purple authored effect references: 0x80B9FDBE and 0x80C220EB

The master timeline 0x80FCCE87 and the three one-member cast handles are different concepts. Calling both of them “Scene 1/2/3” caused repeated confusion. In this document:

- **Opening master timeline** means 0x80FCCE87.
- **Scene 1, Scene 2, Scene 3** mean the user-tested cast handles 0x80EC0F0E, 0x80EC0FA8, and 0x80EC0FA6.
- **Open portal scene** means 0x80C51CBE.

### 6.4 Launch and local-session bootstrap

The working mission launch is not just a package-name override. The observed bootstrap is:

1. The activity selection manager creates a private-current request for activity 299 / mission_scot.
2. The forced service-6 selection replaces absent bubble/spawn fields with bubble 15, slice set 120, and spawn hash 0x4AB3287A.
3. The local solo activity-script manager initially has no assigned authority entry.
4. A local slot-0 bootstrap creates one assigned entry and allows the native authority refresh gate to proceed.
5. Destination package registration resolves mission_scot and the selection state machine advances through its normal states.
6. The private activity join creates the host session.
7. The host-ready latch is set from source private_activity_join.
8. The first phase-1 roster packet registers the activity groups, grants bubble 15, and binds region/slice 120.
9. Later phase-2 packets seed the authored objects.

Relevant live client settings:

- force_join_request_ready = true
- pin_replicated_record = true
- seed_authored_sensors = true
- hold_spawn = true
- spawn_hold_ms = 30000

The spawn hold uses awaiting_client_sync while the world and authority graph load. It is not a substitute for mission readiness; it prevents the player from outrunning a still-empty authority graph.

General lesson:

- forced selection, package registration, local authority assignment, host join, roster registration, object seeding, and spawn release are separate gates;
- skipping directly from package selection to “ready” can produce a loaded world with no mission runtime.

## 7. Scene discoveries

### 7.1 The strongest localization came from scene isolation

The controlled A/B runs established:

- Suppress all three cast scenes: no mission actors appear, while Ikora dialogue/audio still plays.
- Scene 1 only: the genuine authored opening animation plays perfectly; no T-pose, no purple aura, and no purple beams.
- Scene 1 plus Scene 3, with Scene 2 skipped: the non-VFX presentation is visually correct and complete; the portal aura/beams are absent.
- Scene 2 only: the unwanted T-pose/body presentation and the purple aura/beams appear together.
- All native scenes: the full sequence runs, but Scene 2 contributes the wrong visible body and detached VFX.

This gives a binary content boundary:

- Scene 1 and Scene 3 are not the source of the purple defect.
- Scene 2 owns both the wanted purple presentation and the unwanted visible model presentation.

### 7.2 Scene 1: 0x80EC0F0E

Confirmed behavior:

- It is a native one-member cast using Ikora entity 0x80EC0F27.
- It plays the real animation immediately preceding/causing the purple-line moment.
- In isolation it has no T-pose.
- In isolation it has no purple aura or beam output.
- It therefore provides the correct visible animated performer but does not independently own the purple type-23 events.

Interpretation:

- Scene 1 is the animated action segment.
- Re-running or forcing Scene 1 alone cannot make Scene 2's effect attach to it, because its timeline does not own the purple effect event.
- A future durable VFX correction must either make Scene 2 consume Scene 1's live pose interface or reproduce the intended authored cross-scene handoff at an earlier provider layer.

### 7.3 Scene 2: 0x80EC0FA8

Confirmed behavior:

- It is a native one-member cast using Ikora entity 0x80EC0F27.
- It owns the visual moment containing the purple aura and beam output.
- In isolation it produces the T-pose/static body presentation and purple effects.
- Suppressing Scene 2 entirely removes both the bad body and the wanted effects.
- Its presentation component is 0x80EC13A2.
- Its model graph builds a primary body, mesh/cloth, and a separately nested head.
- Its terminal actor-retirement transition is the lifecycle edge now suppressed to retain the effect indefinitely.

Interpretation:

- Scene 2 is an effect/presentation segment, not simply “the animation.”
- Its actor graph is required for authored events even though its visible body is undesirable in the reconstructed client state.
- The correct compromise is to keep the scene, actor, animation graph, events, and VFX native while suppressing only its model construction.

### 7.4 Scene 3: 0x80EC0FA6

Confirmed behavior:

- It is a native one-member cast using Ikora entity 0x80EC0F27.
- It starts later than the first two cast sequences.
- Scene 1 plus Scene 3 gives the correct non-purple presentation.
- It remains enabled in the current build.
- Scene 3 can activate while Scene 2's terminal retirement is retained.

Strong inference:

- It is the final/persistent pose or post-action handoff segment.
- It is part of normal mission progression and should not be disabled merely to retain Scene 2.

Important nuance:

- Scene 3 is “persistent state” in the presentation sense, but the purple effects are authored by Scene 2. Enabling Scene 3 does not automatically transfer Scene 2's VFX to Scene 3's body.

### 7.5 The open portal scene: 0x80C51CBE

Confirmed behavior:

- This is separate from the three Ikora cast handles.
- It supplies the open Infinite Forest portal/interior presentation.
- The open portal can be visible while the closed lattice wall is missing.

Therefore:

- “Open portal visible” and “closed wall existed first” are separate content states.
- The purple beam transition does not create the portal scene from nothing; it coordinates state changes among already authored scene/device objects.

### 7.6 Dialogue and actors are separable

When all three mission actor casts were suppressed:

- Ikora's dialogue/audio still played.
- No mission actors appeared.

This proves that:

- the dialogue timeline/sensor path is not dependent on successful cast model construction;
- hearing the voice line is not proof that an actor or its presentation components exist;
- future debugging should treat audio, scene authority, cast scheduling, model construction, and VFX as independent checkpoints.

## 8. Native cast and actor lifecycle

### 8.1 The three actors are intentional scene casts

The factory recorder found three one-member native scene scheduler constructions, all using Ikora entity definition 0x80EC0F27 and the same authored root region.

They are not three repeated emissions from the network type-1 squad spawner. Their scene handles are distinct and they occur at scene-defined times.

Older recorder labels Actor 1, Actor 2, and Actor 3 mean “first, second, and third captured factory sequence.” They are not official content names and must not be equated with:

- Scene 1/2/3 semantic roles without the scene handle;
- transform selector 1/2/3;
- roster slot indices;
- renderer indices.

### 8.2 Shared authored root

The factory descriptors repeatedly used a common position near:

- X: 347.397
- Y: 249.845
- Z: 99.459

The live cinematic bodies move after construction. Factory descriptor position is therefore useful for content correlation but is not the live animated pose.

### 8.3 Runtime handles are unstable

Object handles varied between runs. Generation is encoded into the full handle, and object-table indices can be reused.

Correct correlation key:

~~~
definition tag
+ scene handle
+ full runtime handle/generation
+ creation timestamp/order
~~~

Incorrect correlation key:

~~~
low handle index alone
or “the first Ikora I saw”
or a pointer suffix such as BC30/AAD0
~~~

### 8.4 The real extra-spawner problem

An earlier active type-1 spawner body caused a third persistent T-pose actor in addition to the native scene performers.

That finding is separate from the three native cast sequences:

- Native Scene 1/2/3 casts are authored and intentional.
- The server-forced type-1 spawner activation was an accidental duplicate in this reconstruction.

Current mission staging intentionally skips the Spawner activation step and lets the native scene scheduler create its actors.

## 9. Scene 2 model suppression

### 9.1 Why broad suppression failed

Broad approaches were tested:

- disabling Scene 2 removed the wanted effects;
- presentation-component suppression captured no useful renderer objects at the expected boundary;
- forcing the presentation body state off did not change the visible model;
- renderer-owner correlation did not prove a unique actor-to-effect ownership link.

These were the wrong abstraction levels.

### 9.2 Exact model components

Scene 2's visible presentation is built through three resource-specific paths:

- Primary model definition 0x80EC0F17 through native builder +0x1225BB0.
- Mesh/cloth model definition 0x80EC0F1D through native builder +0x11761F0.
- Nested head model definition 0x80F2EC2F through native builder +0x1225BB0.

The head is a separate child model. That is why the first successful body suppression left a floating head.

### 9.3 Current suppression behavior

The current hook suppresses only those three model-builder dispatches while all of these remain native:

- Scene 2 cast creation;
- actor object and behavior graph;
- scene animation/event timing;
- authored type-23 callbacks;
- transform provider resolution;
- purple effects;
- scene completion/handoff logic, except the separately controlled terminal retirement.

Current flag:

- kEnableOmegaSceneTwoModelSuppression = true

This is confirmed visually:

- body disappeared after suppressing body/cloth;
- head remained;
- adding the exact head model suppression removed the head;
- the purple effects remained.

This establishes a reusable rule: if a scene owns both wanted events and an unwanted character render, suppress the exact model construction, not the scene or actor.

## 10. Infinite Scene 2 persistence

### 10.1 User requirement

The requested behavior was:

- leave the unresolved purple VFX as-is for now;
- re-enable Scene 3;
- keep Scene 2's visual state alive indefinitely even when Scene 3 begins;
- continue mission progression.

### 10.2 Implemented boundary

The hook targets native scene transition/retirement function +0x58B9A0.

It filters:

- scene handle 0x80EC0FA8;
- terminal transition low byte 0xFF;
- only the actor-retirement action.

It does not suppress:

- Scene 2's authored event completion;
- Scene 3 activation;
- mission script updates;
- all scene retirement globally.

Current flag:

- kEnableOmegaSceneTwoInfinitePersistence = true

### 10.3 Why a completion substitute is required

The native actor release had become the strongest proven scene-completion edge. The host uses that edge only after:

- the opening trigger was entered;
- the 140-bit scene handoff phase was observed.

Suppressing Scene 2's terminal retirement intentionally removes that allocator-lifecycle signal. The current implementation therefore schedules the same deferred host completion latch when it skips the terminal retirement.

The latch is applied from a normal gameplay update, outside allocator teardown. This avoids changing host mission state from inside a sensitive object release path.

### 10.4 Current result

Confirmed:

- Scene 2 is retained.
- Scene 3 is enabled and runs.
- The open portal presentation remains.
- Mission completion staging can continue past the retained scene.

This is the correct current compromise while mission progression work takes priority over the unresolved VFX anchor.

## 11. Purple VFX: what is proven

### 11.1 The effects are real authored scene events

The opening master timeline 0x80FCCE87 contains two real type-23 events:

- effect reference 0x80B9FDBE;
- effect reference 0x80C220EB.

Both callbacks execute. Both create native effect objects. The purple defect is therefore not caused by:

- missing event registration;
- missing effect references;
- the event callback never firing;
- a zero event selector;
- the entire Scene 2 visual path being absent.

### 11.2 Authored selector and source row

Both purple events resolve:

- selector/index: 1;
- route: direct transform bank;
- source kind: 2;
- output start/count: 1 through 1;
- source index: 1;
- flags: 0x3C;
- cast index: -1.

The exact bank entry selected by the native path changes frame by frame. It is not a constant zero transform.

The transform selector number is a transform-bank index. It is not an actor ordinal. “Selector 2” does not mean “Actor 2.”

### 11.3 Recovered native call chain

The relevant native boundaries are:

- Type-23 event callback: +0x11EE970
- Transform resolver: +0x58A150
- Transform bank writer: +0x58EB40
- Source-kind-2 deeper resolver: +0xA1F360
- Provider resolver: +0xA1F640
- Pose candidate resolver: +0xA1E8A0
- Pose socket tail dispatch: +0xB314A0
- Corrected final per-effect transform composer: +0x11F0510
- Effect creation caller: +0x1206E14
- Per-frame effect update caller: +0x121074F

These RVAs apply to the current analyzed client image. They must be revalidated after a game executable change.

### 11.4 Pose-provider candidates

Repeated captures found:

- Purple VFX candidate row: index 7
- Purple selection key: 0x98DA9A6B
- Scene 1 visible-animation candidate row: index 8
- Scene 1 selection key: 0xA02A6431
- Shared authored binding/socket key: 0x15

The pose object seen behind each row differed. Old descriptions referred to pointer endings such as AAD0, B130, or BC30. Those suffixes are run-local addresses and are not durable identities.

The provider component currently identified in source is:

- 0x8161FB60

An earlier exact transform-provenance capture also identified:

- component 0x815B84E3;
- live 0x20-byte transform at component +0x210;
- position within that transform at +0x220;
- one Actor-1-like handle field at +0x43C.

That earlier +0x43C field was a useful upstream hypothesis, but later provider and candidate substitution experiments did not yield a visual correction.

### 11.5 Direct-bank semantics

A guarded callback mutation replaced the callback context object with the second actor and restored it immediately.

The logs proved:

- the mutation applied;
- the nested resolver saw the replacement actor;
- the event still used route=direct_bank;
- object_argument_consumed=no;
- the visual did not move.

Conclusion:

- the callback object's actor field is too late and is not the source selector for this path;
- repeating that mutation cannot solve the problem.

### 11.6 Complete low-level effect set

The low-level effect provenance recorder identified exactly four effect handles constructed at the two purple event timestamps:

- 0x80C71D8E
- 0x80C71D8A
- 0x80F1FCCA
- 0x80C71D70

These are the complete exact filter used by the final transform-composer experiment.

### 11.7 Final effect-composer +10 Z experiment

The corrected function +0x11F0510 writes the final 0x20-byte transform at the start of each effect object.

The current code adds +10 to:

- current position Z at effect +0x08;
- previous position Z at effect +0x18.

For all four exact handles, the logs prove successful write and readback. Representative results:

- native current/previous Z approximately 102.566;
- shifted current/previous Z approximately 112.566;
- applied=yes.

The user then observed no visible movement of the purple beam/aura.

This is a hard negative result:

- those vectors are real and live;
- the code reached the exact four effects;
- the mutation persisted through readback;
- those vectors alone are not sufficient to reposition the final visible composite.

Possible explanations still open:

- a later renderer/composite path derives the visible geometry from another transform;
- these objects are subeffects while a parent ribbon/beam uses a separate anchor;
- the visible beam endpoint/origin is generated from a second runtime structure;
- the renderer consumes cached data outside the mutated current/previous pair.

What is not justified:

- trying +20, -10, or another arbitrary coordinate at the same fields;
- concluding the hook “did not run”;
- assuming the apparent beam origin is the effect object's displayed current position.

### 11.8 Current active VFX flags

Current source:

- kEnableKind2ProviderAb = false
- kEnableOmegaPurplePoseRebind = false
- kEnableOmegaSceneTwoPresentationSuppression = false
- kEnableOmegaSceneTwoBodyStateSuppression = false
- kEnableOmegaSceneOneWithVfxAb = false
- kEnableOmegaIkoraSceneTwoOnlyAb = false
- kOmegaPurpleDirectEffectWorldZOffset = 10.0

The candidate-8 pose rebind is disabled because it was visually disproven. The +10 direct-effect mutation remains enabled as experimental residue and should be removed or changed to observe-only before later VFX work resumes.

## 12. Purple VFX: failed and disproven routes

### 12.1 Broad scene or actor suppression

Result:

- can remove actors or damage the start of the sequence;
- also removes wanted events when Scene 2 is suppressed;
- does not establish the VFX source binding.

Conclusion:

- actor visibility and effect anchoring are separate problems.

### 12.2 Treating all Ikoras as squad-spawner duplicates

Result:

- native scheduler still creates three intentional one-member casts;
- aggressive suppression damaged the scene;
- a separate server-forced type-1 actor really was a duplicate, but that does not make the three scene casts duplicates.

Conclusion:

- do not delete native cast members based only on visible actor count.

### 12.3 Forcing Scene 1 again

Result:

- Scene 1 plays the right animation but does not own the purple events.

Conclusion:

- replaying it cannot redirect Scene 2's effect provider.

### 12.4 Binding by actor number or selector number

Result:

- selector 1 is a transform-bank entry;
- actor/factory capture numbers are unrelated.

Conclusion:

- never equate selector 2 with Actor 2.

### 12.5 Callback object rewrite

Result:

- applied and restored;
- direct-bank path ignored the object argument;
- no visual correction.

Status: disproven.

### 12.6 Candidate-8 / Scene-1 provider substitution

Several progressively narrower provider substitutions were tested:

- creation-object retention;
- dynamic provider location;
- fixed-offset fallback;
- mutable header fallback;
- candidate row 7 to row 8 substitution at +0xA1F640/+0xA1E8A0;
- structural/ownership guard variations.

The tests either failed their guarded applicability checks or reported applied without visually attaching the effects to the performer.

Status:

- the current source explicitly disables the route;
- do not restart it without new evidence identifying a different consumer.

### 12.7 Renderer ownership rewrites

An opaque created owner was captured in one run, but it did not exactly match Actor 1, Actor 2, or Actor 3.

Renderer wrapper hooks were valuable for separating presentation, but they did not prove that a renderer owner pointer controls the beam source.

Status: unproven and deprioritized.

### 12.8 Scene-bank coordinate offsets

Earlier attempts to add world offsets at scene-bank/provider fields either did not apply at the live visual consumer or produced no visible movement.

Status: disproven at the tested layers.

### 12.9 Final effect coordinate offset

The +10 Z write definitely applied to all four exact live effect objects, yet the visible output did not move.

Status: disproven as a sufficient visual fix.

### 12.10 Generic portal visual as the only purple path

The 0x8080992F type-4 visual path is real for separately published portal visuals, including D00142CF slots 2 through 4. It does not by itself explain the dynamic authored Scene 2 type-23 effect chain.

Conclusion:

- the opening scene and post-scene portal authority can both produce purple-looking output;
- do not collapse them into one assumed object.

## 13. Mission scripting discoveries

### 13.1 Two core mission runtime slots

The roster contains:

- Type 18: activity script, auth schema 0x80809919
- Type 35: mission director, auth schema 0x808099BF

Both embed shared mission state schema:

- 0x808099C4

The shared state is not safely initialized by zero-fill.

### 13.2 Shared mission state and the one-year horizon

The native constructed shared state includes:

- active latch;
- lower bound 0;
- upper horizon 0x0000134F00C00000;
- remaining wide fields neutral;
- final 32-bit scalar neutral.

The upper horizon corresponds to the engine's single-precision conversion of 365 days through its tick rate.

An all-zero shared-state tail creates an empty validity window. A mission object can appear structurally active while never being valid.

This was a universal schema lesson: constructor defaults often carry semantic ranges that cannot be reproduced by blindly clearing the body.

### 13.3 Activity script scalar

The active type-18 body contains:

- shared state;
- one schema-specific flag;
- one biased signed 32-bit scalar.

The reconstructed scalar now means:

- 0: host not ready / script inactive
- 1: host ready, opening prepared, trigger not yet entered
- 2: opening trigger entered and native scene sequence active
- 3: scene completion committed after portal authority handoff

The signed scalar uses a -2^31 wire bias. Wire 0 does not mean stored 0. The canonical wire value for stored 0 is 0x80000000.

### 13.4 Mission director

The type-35 outer variant remains neutral:

- two bools neutral;
- two bias-1 two-bit enum fields encoded with safe neutral values;
- embedded shared-state active latch follows host readiness.

The investigation did not find a reason to invent a nonzero outer mission-director variant. The critical change was correct shared-state construction and activation.

### 13.5 Host readiness

The first host-owned readiness edge comes from the activity/session reestablishment path after authored identity activation.

The state is scoped to:

- an active forced destination;
- package mission_scot;
- the current mission session.

The readiness latch enables:

- mission director active;
- activity script flag;
- script state 1;
- later acceptance of the Omega opening trigger.

### 13.6 The opening trigger

The actual trigger is a client sense update for:

- registry: 0xD00142CF
- slot type: 30
- slot index: 20
- body width: 100 bits
- state: entered edge

The server parses this client activity message and calls mark_omega_opening_triggered().

The server logs:

~~~
omega_opening_transition
key=0xD00142CF
slot=30/20
activity_script_state=2
~~~

This is the real player-entered volume boundary, not an arbitrary timer.

### 13.7 Scene phase and handoff

The client also reports the Scene authority record:

- registry: 0xD00142CF
- slot type: 43
- slot index: 1

Two observed shapes matter:

- 108-bit intermediate phase;
- 140-bit handoff phase.

The 140-bit phase arms the handoff. It does not itself mean the cinematic has completed.

This distinction fixed an earlier premature-completion model.

### 13.8 True scene completion

The strongest native completion edge was:

- opening trigger already latched;
- 140-bit scene handoff already armed;
- cinematic actor then reaches native release/retirement.

Because allocator teardown is sensitive, the release probe does not directly advance host state. It schedules a deferred completion, and a normal gameplay update calls mark_omega_scene_completed().

With infinite Scene 2 persistence enabled, the filtered skipped terminal retirement schedules the same deferred completion.

### 13.9 Completion ordering

The host intentionally:

1. keeps activity script state 2 while publishing the portal/gate/monitor delta;
2. waits until that packet has been sent;
3. publishes state 3 in the following Completed packet;
4. transitions to Settled keepalives.

This prevents the mission script from observing “complete” before the client receives the presentation/state handoff.

## 14. Omega opening packet state machine

The connection-owned stages are:

- 0 — None
- 1 — Baseline
- 2 — Scene
- 3 — Ready
- 4 — Triggered
- 5 — Spawner, retained only for compatibility with older in-flight builds
- 6 — Portal
- 7 — Completed
- 8 — Settled

### 14.1 None

- No targeted Omega delta.
- The full roster and initial authority state can still be registering.

### 14.2 Baseline

- Connection bookkeeping only.
- Encoded as stage None so the complete authored seed is sent once.
- Includes the inactive Scene selector needed for the native graph to resolve.

### 14.3 Scene

- Sends the targeted Scene record.
- Sets omegaSceneTransition.
- Refreshes/pre-resolves the opening Scene authority without activating the old type-1 duplicate route.

### 14.4 Ready

- Opening setup is committed.
- Waits for the actual type-30/index-20 player trigger.
- The current implementation also attempts to activate the destination-owned closed wall here.
- That wall attempt is encoded correctly but does not materialize because its registry entries do not exist in the mission runtime.

### 14.5 Triggered

- Script state becomes 2.
- Native scene sequence is in progress.
- Waits for real scene completion.

### 14.6 Spawner

- Legacy compatibility stage.
- The current route does not actively seed the type-1 spawner because the native scenes already create the cast.
- Any older in-flight connection at this stage advances directly to Portal.

### 14.7 Portal

- Publishes post-scene portal visuals and mission boundary objects.
- D00142CF type-4 indices 2 through 4 are active.
- Gate, engagement, and selected monitor records are advanced.
- Script state is deliberately still 2 in this packet.

### 14.8 Completed

- Script state becomes 3 after the Portal packet.

### 14.9 Settled

- Stable keepalive state.
- Phase 1 can remain present.
- No repeated phase-2 edge is emitted.

### 14.10 Transaction safety

The stage is connection-owned. A failed or discarded frame rolls the stage back through roster publication so the same packet can be retried.

This avoids advancing server bookkeeping when the client never received the corresponding authority delta.

## 15. Activity roster and authority reconstruction

### 15.1 Message type and two-phase model

Activity message type 5 is sensor_auth_update.

It has two semantically different phases:

1. **Phase 1:** register roster groups, their slots, bubble-local ownership, and grants.
2. **Phase 2:** send per-object auth/sense resets and state bodies for already registered runtime objects.

Phase 1 naming a group does not guarantee that phase 2 has an object to update.

This is the exact boundary exposed by the failed closed-wall experiment.

### 15.2 Patch epoch

The client supplies a patch epoch in message 52. The server must echo it exactly.

If the epoch is wrong:

- phase 2 is skipped;
- no object authority state is applied;
- debugging the individual body becomes meaningless.

### 15.3 Top-level and bubble-local groups

The phase-1 roster layout requires:

- all top-level groups first;
- bubble-local groups after the top-level boundary;
- a separate bubble sub-block naming which local keys belong to bubble 15.

Inserting a foreign group after the boundary can accidentally turn the first bubble-local group into a global group or otherwise corrupt ownership.

The wall experiment therefore inserted 0x4B946B28 exactly at the top-level/bubble-local boundary.

### 15.4 Normal mission_scot roster groups

The reconstructed mission roster normally contains these authored keys:

- 0x4786C0E0
  - type 16 package/content state
  - type 35 mission director
  - type 18 activity script
  - type 17 lifetime
  - type 41 queue state
  - many type 13 player participation records
- 0x82FB58B7
  - type 68 directive sensor
  - type 11 music sensor
  - type 53 dialogue sensor
- 0xBA5F26EF
  - type 4 visual
  - type 23 controller
  - type 70 helper/state
- 0xD00142CF
  - type 1 spawner
  - type 43 Scene authority
  - type 4 visual records
  - type 26 holds
  - type 23 gate/controller
  - type 70 engagement
  - type 31 points
  - type 30 monitors/volumes
  - type 34 helpers
  - additional authored support slots
- 0xF7A6CE7F
  - two type 30 records
  - two type 57 records

The foreign wall test temporarily adds:

- 0x4B946B28
  - type 23/index 2
  - type 4/index 25

The exact type sequences printed by the current roster, in each group's explicit authored-index order, are:

~~~
4786C0E0:
16,35,18,17,41,
13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13

82FB58B7:
68,11,53

BA5F26EF:
4,23,70

D00142CF:
1,43,4,4,4,26,26,4,4,4,4,4,4,4,4,4,23,70,31,31,30,34,34,34,30

F7A6CE7F:
30,30,57,57

Temporary foreign group 4B946B28:
23,4
~~~

The display above is a type sequence. The implementation still carries separate explicit indices; repeated type 4 entries must not be assigned indices by counting their position in this text.

### 15.5 Declared objects versus runtime objects

The normal mission roster advertises 56 slots but produces 53 runtime authority objects.

The difference is intentional:

- D00142CF authored indices 21 through 23 do not have client-backed components.

With the two foreign entrance objects added:

- advertised objects increase from 56 to 58;
- runtime object count remains 53.

That is decisive evidence that neither foreign object materialized.

### 15.6 Slot ordinal is not slot index

A roster group's array position is not necessarily the authored object index.

The implementation now carries explicit slotIndices alongside slotTypes and slotFlags.

Without this:

- a body can be perfectly encoded for the wrong authored object;
- sparse groups shift every slot after the first gap;
- logs that print only ordinal hide the mismatch.

### 15.7 State sequence is object generation

stateSequence controls the lifetime generation of the roster-owned runtime objects.

Changing it:

- tears down existing objects;
- rebuilds them;
- is not a normal auth-body revision counter.

The mission director/activity-script transition originally advanced it and destroyed the objects before their replacement body could update them.

Current rule:

- keep stateSequence stable for state-body changes;
- change it only when intentionally rebuilding the roster generation.

### 15.8 Type-17 lifetime safety

The type-17 spawn gate indexes an unbounded jump table.

Only these lifetime states are known safe:

- 3
- 6
- 10

Any other value is not merely rejected; it can jump through an invalid target.

The current roster uses 3, which was observed in a live activity.

### 15.9 Type-13 participation and player identity

Type 13 binds the player to the activity and region.

Key findings:

- the join request may carry a short identity form;
- the authority body needs the full authored character SOID;
- only a group that truly owns a type-13 participation slot should carry the player key;
- publishing the player key on every group does not create a valid binding;
- optional/presence fields and signed biases must be encoded exactly.

The current implementation matches the join character's low identity half to the account's full authored SOID.

### 15.10 Auth and sense flags

Roster slot flags:

- bit 0: sense reset/body path
- bit 1: auth reset/body path

The object block includes:

- object presence;
- registry key;
- biased 7-bit slot type;
- biased 16-bit slot index;
- remainder length;
- auth reset bit;
- auth body presence/body;
- sense reset/body presence as flagged.

Important framing facts:

- the first auth reset bit must be 1; zero sends decode into a throwaway mirror;
- sense-present 1 consumes another 35 bits in the tested shape;
- incorrect remainder lengths make all following records appear corrupt;
- all-zero bodies are frequently invalid because of biases, ranges, selectors, and constructor defaults.

## 16. Recovered authority schemas

### 16.1 Type 4 visual — schema 0x8080992F

After the object block supplies the schema root/self bit, the active body is 253 bits.

Recovered field order:

1. u32
2. u32
3. active bool
4. fallback/resolve bool
5. u32
6. nested 0x80809C42 key:
   - u32 registry/hash
   - biased u7 type
   - biased u16 index
7. primitive kind 13:
   - u32 X bits
   - u32 Y bits
   - u32 Z bits
8. bool
9. inherited two-bit state
10. optional u32 presence/value

Key behavioral findings:

- runtime active latch is at +0x08;
- the fallback/resolve field is required for authored/reference resolution;
- leaving it clear produced an uninitialized transform;
- primitive kind 13 is three u32 values, not one;
- the inherited trailer must be present and aligned.

### 16.2 Type 43 Scene authority — schema 0x8080626B

Body:

- u32 selector
- active bool
- nested four-bit mode

The inactive selector must be seeded before activation. Sending only an active edge without the earlier selector left the native graph unresolved.

Current opening Scene selector:

- 0x80EC0F96

### 16.3 Type 1 spawner — schema 0x80807EC9

Recovered active form:

- 355 bits after the object root;
- several optional/presence fields;
- six requested member counts;
- generation state 1;
- mode 0;
- hidden squad-device key through type 66/index 0;
- nested generation schema 0x80807ED2;
- requested-count schema 0x80807ECF;
- key schema 0x80809C42.

The active test requested one member from the first authored placement class and zero from the remaining five.

Result:

- it created an extra persistent T-pose actor because native scenes already constructed the relevant cast.

Current status:

- schema is understood enough to encode;
- the mission route intentionally does not activate it.

### 16.4 Type 26 holds

No usable client runtime registration was observed for the tested active route. Attempting active bodies stopped the decoder/progression.

Current status: avoid in targeted portal deltas.

### 16.5 Type 31 points

Tested active forms also stopped the packet path.

Current status: avoid until its exact schema and client runtime registration are independently recovered.

### 16.6 D00142CF slots 7 through 15

These records are named:

- _o_gateway_extended_push[0] through [8]

They have:

- authored_count=0;
- selector route no_authored_entries.

They are wrapper/control records, not nine hidden wall visuals.

This route is closed. Do not retry those slots as the entrance wall.

### 16.7 Sensor bodies

Recovered/seeded sensor categories include:

- type 68 directive;
- type 53 dialogue;
- type 11 music.

These can initialize and operate separately from cast model construction, consistent with the no-actor/dialogue-still-plays A/B.

## 17. The closed Infinite Forest entrance wall

### 17.1 Visual target

Retail shows a bright, closed triangular Vex lattice across the entrance when the player arrives.

During the Ikora sequence:

- purple beams strike it;
- the wall breaks/transitions away;
- the open portal backdrop remains.

The closed lattice and open portal are not the same object/state.

### 17.2 Exact package discovery

The wall is destination-owned rather than mission_scot-opening-owned.

Root destination registry:

- object/tag: 0x80F4708B
- registry key: 0x4B946B28
- declared slots: 553

Entrance controller:

- authored name: infinite_forest_entrance_device
- component definition: 0x80F46EC9
- wrapper/reference: 0x80F46ECB
- slot: type 23/index 2
- name hash: 0xF939AAF5

Placed pulse wall:

- authored name: infinite_forest_entrance_pulse_object
- component definition: 0x80F46F0E
- reference: 0x80F46F0F
- wrapper: 0x80F46F10
- slot: type 4/index 25
- name hash: 0x1BF550F4
- entity definition: 0x80F44EED

Authored world transform:

- position: (361, 250, 106)
- quaternion: (0, 0, -0.707106769, 0.707106769)
- scale: 1

The type-4 definition:

- uses auth schema 0x8080992F;
- has one authored entry.

Other colocated destination objects exist near the same transform. Their exact role in the closed/open transition is not yet proven, so the pulse object must not be described as the only possible geometry without qualification.

### 17.3 Implemented foreign-registry test

The server:

1. inserted registry 0x4B946B28 as a top-level group;
2. advertised only exact descriptor-backed objects type 23/index 2 and type 4/index 25;
3. sent the type-4 active Ready body using the recovered 253-bit schema;
4. retained the mission's normal bubble-local ordering;
5. logged the exact record and bit boundaries.

### 17.4 What the logs prove

The server repeatedly reports:

~~~
key=0x4B946B28
slot=4/25
body_bits=253
expected_end matches actual end
result=ok
~~~

The client group decoder accepts:

- registry key 0x4B946B28;
- biased type decoding to type 4;
- biased index decoding to index 25.

However, the client authority publisher repeatedly reports:

~~~
activity_authority_publish
count_before=0
count_after=0
~~~

for the foreign group.

There is no visual-selector/application record for component 0x80F46F0E.

The total roster changes from 56 to 58 advertised records while the runtime stays at 53.

### 17.5 Exact failure boundary

Confirmed:

- content discovery is correct;
- registry key is correct;
- slot type/index are correct;
- the group is inserted at the correct phase-1 boundary;
- packet framing is correct;
- body width and schema are correct;
- the client decodes the key/type/index;
- no runtime object is constructed for either foreign slot.

Therefore the failure is:

**The destination registry was named on the wire but was never mounted/instantiated into mission_scot's active authority container.**

The client cannot apply an auth body to an object that does not exist.

This explains the user's final observation: “there is nothing.”

### 17.6 Why more packet tuning is not justified

Changing any of these cannot solve the current failure:

- active flag;
- transform bits;
- sense flag;
- repeated sends;
- stateSequence;
- index 24/26 guesses;
- more Ready keepalives.

None of those create the missing runtime definition.

### 17.7 Remaining wall routes

#### Route A: mount the destination registry

Goal:

- make registry object 0x80F4708B participate in mission_scot's active authority/runtime build;
- allow the native client publisher to create type 23/index 2 and type 4/index 25;
- then reuse the already-correct authority bodies.

Advantages:

- most faithful to retail;
- preserves controller relationship and authored state transitions;
- likely brings colocated supporting objects with correct dependencies.

Risks:

- registry mounting may be destination/bubble scoped;
- importing all 553 slots blindly would be unsafe and unnecessary;
- package load availability is not the same as authority registration.

Status: unimplemented.

#### Route B: direct native entity instantiation

Goal:

- instantiate entity 0x80F44EED directly at the authored transform;
- later hide/retire it at the opening transition.

Known native factory boundary from scene hooks:

- function: +0x56D9B0
- arguments conceptually include result, descriptor, table, record;
- normal scene calls use table 0xFFFFFFFF and record -1;
- descriptor holds definition at +0x00, quaternion at +0x10, position at +0x20.

Advantages:

- narrow;
- bypasses the absent foreign authority container;
- adequate if the immediate goal is a visible collision/presentation wall.

Risks:

- the pulse entity may expect controller or parent state;
- direct entity construction may not create the exact authority-backed transition behavior;
- lifetime/cleanup must be designed;
- collision and visuals may be separate components.

Status: statically plausible, not authored or tested.

#### Route C: rebind a mission-owned runtime slot

Goal:

- use an already materialized mission_scot type-4 runtime record;
- substitute the exact wall definition/entry under strict Omega filters.

Advantages:

- avoids mounting a new registry;
- preserves a known authority object lifetime.

Risks:

- can steal or corrupt an existing portal visual;
- definition/component compatibility is not proven;
- must not reuse D00142CF slots 7 through 15, which have no authored entries.

Status: hypothetical and lower priority than Routes A or B.

### 17.8 Recommended next wall experiment

If a faithful mission system is the priority, investigate Route A first.

If a visible spawn-time wall is needed quickly to support mission progression testing, Route B is the most direct bounded experiment:

1. Build an exact descriptor for 0x80F44EED.
2. Use the authored quaternion and position.
3. Call the validated native factory only after the Lighthouse world is ready.
4. Record returned handle, components, render presence, and collision.
5. Do not wire its destruction to the purple VFX yet.
6. If the entity does not materialize, stop the direct-factory route and inspect missing parent/controller dependencies.

## 18. Universal discoveries applicable to other missions

### 18.1 Content discovery is not runtime existence

The complete ladder is:

~~~
package tag discovered
  -> scenario/registry selected
  -> roster group registered
  -> runtime authority object instantiated
  -> auth/sense body applied
  -> component state changes
  -> renderer/gameplay consumer materializes output
~~~

Proof at one rung is not proof of the next.

### 18.2 Accepted packet is not visible effect

A packet can:

- encode successfully;
- have correct bit length;
- decode to the correct registry/type/index;
- still do nothing because no runtime object exists.

Always log runtime count before/after and the actual component apply callback.

### 18.3 Scene systems are layered

A cinematic is not one monolithic object. The Omega work separated:

- scene authority record;
- master timeline;
- one-member cast scheduling;
- entity construction;
- behavior/animation graph;
- presentation component;
- body/cloth/head model construction;
- authored event callbacks;
- transform bank;
- pose provider;
- low-level effects;
- renderer composite;
- scene phase report;
- actor retirement.

Patch the narrowest layer that owns the unwanted behavior.

### 18.4 Audio is an independent success signal

Dialogue can play with no actors. Treat it as evidence for dialogue/sensor/timeline health, not evidence for cast or model health.

### 18.5 Lifecycle edges are stronger than guessed timers

The correct Omega completion signal was the real native retirement edge after the scene handoff, not:

- a fixed delay;
- first VFX callback;
- 140-bit phase alone;
- Scene 3 construction alone.

Use native lifecycle edges, then defer host mutation out of allocator-sensitive callbacks.

### 18.6 Network generation is not state revision

Changing an object's generation to update one value can destroy the very object meant to consume the update.

Keep lifetime generation stable for ordinary auth deltas.

### 18.7 Constructor defaults matter

Zero-fill is unsafe when schemas include:

- biased signed fields;
- bias-1 enums;
- validity horizons;
- absent-key sentinels;
- inherited trailers;
- optional field presences;
- nested records.

Recover the native constructor or exact retail body.

### 18.8 Sparse slot layouts require explicit indices

Never infer authored slot index from array ordinal. Preserve type, flags, and explicit index as one tuple.

### 18.9 Handles require generation-aware correlation

Object-table indices are reused. Record full handles and creation/retirement timestamps.

### 18.10 Composite visuals can ignore a mutated subtransform

The four-effect +10 test proves a low-level object position can change while the displayed composite does not.

Before another coordinate experiment, identify:

- parent effect;
- ribbon/beam endpoint buffers;
- renderer input;
- any later transform rewrite;
- whether the visible aura and beam are separate consumers.

### 18.11 Model suppression belongs at construction

When a scene's events are wanted but its body is not:

- do not disable the scene;
- do not destroy the actor;
- do not globally disable renderer services;
- suppress exact model resources at exact construction callbacks.

### 18.12 Package availability is not registry mounting

The client can read a destination asset and still omit it from a mission's authority container.

Loading a package and publishing a registry key solve different problems.

### 18.13 A/B isolation beats semantic guessing

The scene-isolation sequence answered in a handful of runs what broad ownership tracing had not:

- Scene 1 = correct animation, no purple;
- Scene 2 = purple plus bad body;
- Scene 3 = correct persistent/final presentation;
- no casts = dialogue still works.

Use controlled binary isolation early.

### 18.14 Bounded experiments need stopping rules

Each mutation should define:

- exact filter;
- expected log proof that it applied;
- predicted visual result;
- one disconfirming result;
- what route is abandoned afterward.

This prevents infinite “one more offset/provider/owner” loops.

### 18.15 Evidence labels prevent accidental certainty

The investigation repeatedly suffered when an address suffix, actor label, or plausible owner field was promoted to fact.

Durable documentation must distinguish:

- confirmed mechanics;
- semantic inference;
- hypotheses;
- disproven routes.

### 18.16 Reusable mission-reconstruction playbook

For another mission, use this order:

#### Phase A: launch truth

1. Recover exact activity package, index, destination, bubble, slice set, and spawn hash.
2. Confirm the selection manager reaches package registration and private host startup.
3. Do not patch scene code while the authority manager still has zero assigned entries.

#### Phase B: roster truth

1. Walk package-authored registry objects.
2. Recover group keys and descriptor-backed slot type/index/flags.
3. Preserve top-level versus bubble-local ownership.
4. Bind the full player SOID only to a real participation group.
5. Prove phase-1 group registration before sending bodies.

#### Phase C: schema truth

1. Identify auth and sense schemas per descriptor.
2. Recover constructor defaults, biases, optional fields, and inherited trailers.
3. Encode one known-safe persistent body first: participation/lifetime.
4. Confirm exact remainder boundaries before adding another object.

#### Phase D: mission runtime truth

1. Initialize activity script and mission director with valid shared state.
2. Keep object generation stable.
3. Find real client sense edges for triggers.
4. Advance one scalar/latch at a time.

#### Phase E: scene truth

1. Seed inactive selectors before active edges.
2. Record master timeline, cast handles, entity definitions, and factory times separately.
3. A/B individual casts.
4. Treat dialogue, animation, model, events, VFX, and retirement as separate outputs.

#### Phase F: presentation truth

1. Identify exact component/resource producing the unwanted output.
2. Suppress or alter the narrowest construction/consumer boundary.
3. Require visual confirmation, not only successful memory writes.

#### Phase G: completion truth

1. Find the native lifecycle edge after the authoritative handoff.
2. Defer host mutation out of sensitive teardown.
3. Publish the destination/presentation delta before marking script complete.

#### Phase H: bounded continuation

1. Archive source, DLL hash, log, and screenshot for every run.
2. State the predicted binary outcome before testing.
3. Abandon a route when its predicted effect fails despite confirmed application.
4. Do not let a diagnostic mutation become an undocumented permanent dependency.

## 19. Experiment ledger

This ledger records the useful behavioral conclusions. Historical source snapshots and logs are preserved under C:\Destiny 2 Development\_analysis.

### 19.1 Scene and cast experiments

#### All exact casts suppressed

Artifact directory:

- _analysis\omega_scene_cast_suppression_20260822

Observed:

- no mission actors;
- Ikora dialogue/audio still played.

Conclusion:

- the three handles are the native mission actor casts;
- dialogue path is independent.

#### Scene 1 only

Artifact directory:

- _analysis\omega_scene_one_only_ab_20260822

Observed:

- correct animation;
- no T-pose;
- no purple aura or lines.

Conclusion:

- Scene 1 is the correct animation segment but not the purple-event owner.

#### Scene 1 and Scene 3 only

Artifact directory:

- _analysis\omega_scenes_one_and_three_ab_20260822

Observed:

- visually correct non-VFX sequence;
- no portal aura/beams.

Conclusion:

- Scene 2 exclusively supplies the wanted purple presentation and unwanted model.

#### Scene 2 only

Artifact directory:

- _analysis\omega_scene_two_only_ab_20260822

Observed:

- T-pose/static Ikora presentation;
- purple aura and beams.

Conclusion:

- direct localization to Scene 2.

### 19.2 Scene 2 presentation experiments

#### Presentation-component renderer suppression

Artifact directory:

- _analysis\omega_scene_two_presentation_suppression_20260822

Observed:

- expected renderer object list was empty at the tested component boundary;
- visible problem remained.

Conclusion:

- wrong suppression boundary.

#### Body-state suppression

Artifact directory:

- _analysis\omega_scene_two_body_state_suppression_20260822

Observed:

- requested state changed;
- visual body remained.

Conclusion:

- presentation state byte was not the final construction gate.

#### Exact model-builder suppression

Observed:

- body/cloth removal worked;
- separate head remained;
- adding 0x80F2EC2F suppression removed the head;
- VFX remained.

Conclusion:

- successful current solution for unwanted Scene 2 model presentation.

### 19.3 VFX provenance experiments

#### Pre-kind-2 recorder

Artifact directory:

- _analysis\omega_vfx_pre_kind2_recorder_20260822

Result:

- localized selector 1, source kind 2, direct-bank route, source index 1;
- callback object rewrite proved irrelevant.

#### Kind-2 provider A/B series

Artifact directory:

- _analysis\omega_vfx_kind2_provider_ab_20260822

Result:

- recovered provider/candidate boundaries;
- tested structural substitutions and fallback locators;
- did not visually attach effects.

Conclusion:

- candidate-8 substitution route is closed in current source.

#### Type-23 suppression experiment

Artifact directory:

- _analysis\omega_vfx_type23_suppression_20260822

Result:

- helped distinguish authored event callbacks from separately published portal/composite visuals;
- did not produce a durable correction.

Conclusion:

- multiple visual layers can coexist.

#### Final low-level effect transform

Evidence:

- exact four handles;
- corrected +0x11F0510 composer;
- +10 Z applied and read back;
- visible composite unchanged.

Conclusion:

- final tested current/previous position vectors are not sufficient visual anchors.

### 19.4 Scene 2 infinite persistence

Observed:

- exact terminal retirement can be skipped;
- Scene 3 still activates;
- mission completion can be latched through the deferred substitute;
- retained VFX state persists.

Conclusion:

- current implementation satisfies the requested “Scene 2 forever while Scene 3 continues” behavior.

### 19.5 Closed-wall foreign registry

Observed:

- exact destination registry and object discovered;
- group and bodies encoded correctly;
- client decodes exact key/type/index;
- runtime count does not increase;
- no component apply occurs;
- no wall appears.

Conclusion:

- packet route is exhausted;
- missing runtime registry/object materialization is the blocker.

## 20. Current source and deployed build state

### 20.1 Deployed binary identity

As of this documentation:

- Live proxy DLL: C:\Destiny 2 Development\bin\x64\steam_api64.dll
- Release build DLL: C:\Destiny 2 Development\Sunrise-src\build\x64\Release\steam_api64.dll
- Size: 13,379,584 bytes
- Timestamp: 2026-08-22 14:56:42.445
- SHA-256: AD55BD4B85F7B1779AC63F98B72413B231D8708615BA9DC72F48BC3876FF4D7F

The two DLLs match exactly.

### 20.2 Main probe source identity

- File: Sunrise\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp
- Size: 616,394 bytes
- Timestamp: 2026-08-22 14:19:04.999
- SHA-256: 7D4C845E46BEBEBD7DB67659721327DF557D443E46141B84F717D841B63A62F9

Important:

- this file is currently untracked by Git;
- ordinary git diff does not preserve or display it;
- its historical experiment snapshots under _analysis are critical evidence.

### 20.3 Repository state

The worktree is heavily modified across:

- scenario/package discovery;
- activity schemas and probes;
- sensor/auth encoding;
- roster publication;
- forced destination state;
- gameplay host/session state;
- activity message routing;
- bootflow hooks;
- settings and project files.

There are also several untracked source files, including:

- activity_spawner_chain_probe.cpp;
- scenario_slot_classification.cpp;
- activity_sense_update_parser.cpp;
- sense_update.h;
- additional bootflow probes.

Do not:

- reset the worktree;
- check out broad directories;
- assume unrelated modified files can be discarded;
- rely on a clean Git history to reconstruct this investigation.

### 20.4 Current flags that matter

Enabled:

- Scene 2 exact model suppression
- Scene 2 infinite terminal persistence
- exact four-effect +10 Z diagnostic mutation

Disabled:

- Scene 2 only A/B
- Scene 1 plus VFX A/B
- presentation-component suppression
- body-state suppression
- kind-2 provider A/B
- purple candidate pose rebind

### 20.5 Logs

Active:

- C:\Destiny 2 Development\bin\x64\Sunrise\logs\sunrise.log

Previous rotated run containing the decisive wall and effect-composer evidence:

- C:\Destiny 2 Development\bin\x64\Sunrise\logs\sunrise.log.old

The active log may be locked while the game is running. Copy/archive it after closing the game before a new diagnostic run.

### 20.6 Shutdown allocator assertion

An index-heap double-free-like assertion was seen when closing the game with the window X.

User observation:

- it is approximately 99% correlated with shutdown.

Current status:

- not treated as the cause of the in-world VFX defect;
- still a real cleanup issue worth addressing later;
- do not use it as evidence that an in-scene experiment failed unless it occurs before shutdown.

## 21. Build and deployment

Canonical command:

~~~powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1"
~~~

The script:

1. locates MSBuild through vswhere;
2. builds Release|x64 by default;
3. expects output at Sunrise-src\build\x64\Release\steam_api64.dll;
4. requires destiny2.exe to be closed before deployment;
5. backs up the current live DLL under .sunrise\backup;
6. deploys to bin\x64\steam_api64.dll.

Warnings:

- the live Sunrise proxy is steam_api64.dll, not Sunrise.dll;
- the real Steam API is stored separately under .sunrise\original and must not be overwritten;
- rerunning the official Sunrise installer can replace this local build because its install-state hash still points to release 0.2.1;
- settings.json can be rewritten on game exit.

After every build:

1. verify the build succeeded;
2. verify live and built DLL hashes match;
3. record the source hash if the untracked probe changed;
4. archive the previous log;
5. launch one controlled test;
6. associate the screenshot/result with that exact DLL hash.

## 22. Relevant source map

### 22.1 Destination and scenario discovery

- Sunrise\src\client\content\activity\activity_tag_reader.cpp
- Sunrise\src\client\content\scenarios\scenario_roster_build.cpp
- Sunrise\src\client\content\scenarios\scenario_roster_groups.cpp
- Sunrise\src\client\content\scenarios\scenario_roster_publish.cpp
- Sunrise\src\client\content\scenarios\scenario_slot_classification.cpp
- Sunrise\src\state\build_data\scenarios\scenario_catalog.cpp
- Sunrise\src\state\build_data\scenarios\definition.h

### 22.2 Roster and sensor/auth wire path

- Sunrise\src\server\bap\encrypted\push\activity\activity_roster_snapshot.cpp
- Sunrise\src\server\bap\encrypted\push\activity\activity_roster_push.cpp
- Sunrise\src\server\bap\encrypted\push\activity\activity_roster_report.cpp
- Sunrise\src\middleware\bap\activity_message\sensor_auth_update.h
- Sunrise\src\middleware\bap\activity_message\activity_sensor_auth_encoder.cpp
- Sunrise\src\middleware\bap\activity_message\activity_sensor_auth_blocks.cpp
- Sunrise\src\middleware\bap\activity_message\activity_sensor_auth_bodies.cpp
- Sunrise\src\middleware\bap\activity_message\activity_sense_update_parser.cpp

### 22.3 Mission host state and client input

- Sunrise\src\state\activity\forced\activity_forced_destination.cpp
- Sunrise\src\state\activity\forced\activity_forced_destination.h
- Sunrise\src\server\bap\encrypted\activity_message\activity_message_route.cpp
- Sunrise\src\server\bap\encrypted\push\activity\activity_keepalive_push.cpp

### 22.4 Scene, actor, and VFX runtime probes

- Sunrise\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp
- Sunrise\src\client\hooks\bootflow\activity_schema_decode_probe.cpp
- Sunrise\src\client\hooks\bootflow\activity_script_event_probe.cpp
- Sunrise\src\client\hooks\bootflow\activity_script_upstream_probe.cpp
- Sunrise\src\client\hooks\bootflow\activity_behavior_condition_probe.cpp

### 22.5 Existing static-analysis outputs

- omega_actor_visual_boundary_out.txt
- omega_rendered_pose_trace_out.txt
- omega_visual_consumer_out.txt
- omega_beam_handlers_out.txt
- omega_scene_scheduler_out.txt
- omega_scene_schema_data_out.txt
- omega_scene_authority_decomp.txt
- omega_scene_persistence_out.txt
- omega_object_lifecycle_out.txt
- omega_object_retirement_out.txt
- omega_post_scene_consumers_out.txt
- omega_visual_auth_schema.txt
- omega_portal_components_out.txt
- omega_portal_schemas_out.txt
- omega_effect_attachment_source_out.txt
- omega_corrected_render_path_out.txt
- omega_corrected_model_builder_out.txt
- mission_schema_data_out.txt
- mission_schema_consumers_out.txt
- mission_runtime_slots_out.txt
- mission_runtime_callbacks_out.txt
- mission_conditions_decomp.txt

### 22.6 Ghidra project and scripts

- GhidraSceneProject\OmegaScene.gpr
- ghidra_scripts\TraceOmegaSceneScheduler.java
- ghidra_scripts\TraceOmegaSceneSchema.java
- ghidra_scripts\TraceOmegaSceneSchemaData.java
- ghidra_scripts\TraceOmegaActorVisualBoundary.java
- ghidra_scripts\TraceOmegaRenderedPose.java
- ghidra_scripts\TraceOmegaVisualConsumer.java
- ghidra_scripts\TraceOmegaPostSceneConsumers.java
- ghidra_scripts\TraceOmegaPortalComponents.java
- ghidra_scripts\TraceOmegaPortalSchemas.java
- ghidra_scripts\TraceOmegaObjectLifecycle.java
- ghidra_scripts\TraceOmegaBodyRenderer.java
- ghidra_scripts\DecompOmegaSpawnerChain.java
- ghidra_scripts\DecompOmegaSceneAuthority.java
- ghidra_scripts\DecompOmegaObjectRetirement.java
- ghidra_scripts\DecompOmegaBeamHandlers.java
- ghidra_scripts\TraceMissionSchemaData.java
- ghidra_scripts\TraceMissionSchemaConsumers.java
- ghidra_scripts\DecompMissionRuntimeSlots.java
- ghidra_scripts\DecompMissionRuntimeCallbacks.java
- ghidra_scripts\DecompMissionConditions.java

## 23. Useful log queries

Run from C:\Destiny 2 Development.

### 23.1 Opening stage and mission scalar

~~~powershell
rg -n "omega_stage=|script_state=|omega_opening_transition|omega_scene_handoff|omega_scene_completed" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.2 Scene cast construction and retirement

~~~powershell
rg -n "omega_ikora_followup|omega_scene_cast|omega_scene_two_persistence|scene_transition_retire|80EC0F0E|80EC0FA8|80EC0FA6" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.3 Scene 2 model suppression

~~~powershell
rg -n "omega_scene_two_model|model_suppression|80EC0F17|80EC0F1D|80F2EC2F" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.4 Purple authored event and provider

~~~powershell
rg -n "omega_scene_vfx_binding|omega_scene_type23_trace|omega_scene_transform_source|omega_scene_transform_writer|omega_scene_pose|80B9FDBE|80C220EB" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.5 Exact four-effect transform experiment

~~~powershell
rg -n "omega_purple_effect_direct_offset|omega_effect_transform_compose|80C71D8E|80C71D8A|80F1FCCA|80C71D70" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.6 Roster and wall runtime materialization

~~~powershell
rg -n "roster_layout|4B946B28|omega_opening_visual_encode|activity_authority_publish|80F46F0E|80F44EED" "bin\x64\Sunrise\logs\sunrise.log"
~~~

### 23.7 Decoder framing

~~~powershell
rg -n "sensor_auth|object_block|body_bits|remainder_bits|patch_epoch|phase=register|phase=seed" "bin\x64\Sunrise\logs\sunrise.log"
~~~

## 24. Non-regression requirements

Any future mission-progression or wall change must preserve:

- correct mission_scot launch into bubble 15;
- player participation and spawn;
- host readiness;
- activity script state 1 before the trigger;
- real type-30/index-20 entered edge;
- activity script state 2 during the opening;
- Ikora dialogue;
- Scene 1 animation;
- Scene 2 events and VFX;
- suppression of Scene 2 body, cloth, and head only;
- Scene 2 infinite persistence;
- Scene 3 activation;
- 140-bit handoff recognition;
- deferred completion outside allocator teardown;
- Portal packet before state 3;
- stable stateSequence across auth-body changes;
- no server-forced duplicate type-1 Ikora;
- no decoder abort caused by unproven type-26 or type-31 bodies.

## 25. Remaining work, ordered by value

### 25.1 Mission progression

This is the user's current priority.

Next work should:

1. identify the post-opening objective/directive state expected after script scalar 3;
2. observe which monitors, points, gates, and destination transitions remain inert;
3. use client sense updates and native scene completion rather than arbitrary timers;
4. add one mission state edge at a time;
5. preserve the working Scene 2/Scene 3 compromise.

### 25.2 Closed entrance wall

The packet route is finished as a diagnostic. Continue only through:

- real destination-registry mounting; or
- direct entity instantiation at the exact authored transform.

Do not spend another run retuning the current type-4 body while runtime count remains zero.

### 25.3 Purple VFX alignment

Deferred by user choice.

When resumed:

1. remove/disable the proven-ineffective +10 mutation;
2. start from observe-only current source;
3. identify the renderer/composite consumer after +0x11F0510;
4. separate aura, beam origins, beam endpoints, and parent effect;
5. require a binary stopping rule before each run.

Do not return to:

- callback object rewrite;
- selector renumbering;
- candidate-8 substitution;
- current/previous effect-position offsets;
- broad actor deletion.

### 25.4 Cleanup and maintainability

- add the main probe and new source files to version control or preserve them in a deliberate patch series;
- separate stable mission functionality from diagnostic hooks;
- remove stale disabled treatments after archiving their source;
- reduce log volume and retain only bounded/change-filtered recorders;
- investigate shutdown allocator cleanup independently.

## 26. Definition of complete

### 26.1 Opening presentation complete

- Closed triangular wall visible on spawn.
- Scene 1 Ikora visible and correctly animated.
- No extra/T-pose Scene 2 body or floating head.
- Purple aura follows the intended visible character.
- Beam origins/endpoints follow the intended animated sockets.
- Wall transitions away at the correct scene moment.
- Open portal remains.
- Scene 3/final presentation remains coherent.

### 26.2 Mission scripting complete

- Activity script advances 0 -> 1 -> 2 -> 3 through real readiness, trigger, and completion edges.
- Mission director remains valid and persistent.
- Correct next objective/directive appears.
- Gate/collision state matches open portal.
- Player can enter and continue.
- Subsequent mission encounters and objectives advance through authored sensors.

### 26.3 Engineering complete

- No duplicate network-spawned Ikora.
- No in-world object-table corruption.
- No packet decoder misalignment.
- No broad hooks affecting unrelated scenes.
- Runtime registration and auth application are separately logged.
- Stable changes are isolated from diagnostic experiments.
- Source, build, deployed DLL, and handoff hashes are reproducible.

## 27. High-confidence “do not repeat” list

- Do not call the three native cast actors accidental spawner duplicates.
- Do not activate the type-1 spawner while native scenes create the cast.
- Do not equate transform selector 1/2 with Actor 1/2.
- Do not rewrite the type-23 callback object again; direct-bank ignores it.
- Do not re-enable the candidate-8 pose substitution without new consumer evidence.
- Do not disable Scene 2 to hide its body; that removes the wanted effects.
- Do not suppress the entire presentation component when exact model builders are known.
- Do not forget the head is a separate child model.
- Do not disable Scene 3 to retain Scene 2; terminal retirement can be filtered independently.
- Do not treat the 140-bit Scene phase as final completion by itself.
- Do not mutate host mission state inside allocator teardown.
- Do not advance stateSequence for an auth-value revision.
- Do not use unsupported type-17 lifetime values.
- Do not zero-fill mission shared state.
- Do not infer slot index from roster ordinal.
- Do not place the player key on groups that do not own participation.
- Do not assume phase-1 registry naming creates phase-2 runtime objects.
- Do not retry D00142CF slots 7 through 15 as wall visuals.
- Do not tune the foreign wall body while count_before/count_after remain zero.
- Do not manually move the same four effect vectors again; +10 applied and did not move the visible composite.
- Do not trust raw pointer suffixes across runs.
- Do not trust git diff to preserve the untracked main probe.

## 28. Compact current truth

mission_scot's launch, roster, mission script, real opening trigger, native Ikora scenes, dialogue, Scene 2 events, Scene 2 model hiding, Scene 2 indefinite retention, Scene 3 continuation, and post-scene completion staging all work well enough to continue mission development.

The purple effects are authentic and execute, but their final visible composite is not controlled by any binding or position field tested so far. That investigation is deliberately paused.

The closed entrance wall has been identified exactly, including its registry, controller, type-4 record, entity, and authored transform. The server publishes a correct packet, but mission_scot never materializes the destination-owned runtime records. The next wall solution must instantiate/mount content, not adjust wire bits.

That is the line between completed reconstruction and remaining work.
