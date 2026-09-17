# Ghost dialogue, mission graphs, SCOT, Towerfall, teleporting, and Infinite Forest seeding

Compiled: **2026-09-03** from the local Dawn source, exported package inventories, and dated investigation handoffs in `C:\Destiny 2 Development`.

This is a consolidated engineering reference for the work represented in this workspace. It is not a claim that the entire retail mission executor has been recovered. No game run, DLL build, or deployment was performed to produce this document. Native addresses below belong to the investigated binary/reference cohort; they are not portable offsets for arbitrary Destiny builds.

## Contents

1. [What we know, and what remains missing](#1-what-we-know-and-what-remains-missing)
2. [The architecture and the different meanings of graph](#2-the-architecture-and-the-different-meanings-of-graph)
3. [Ghost playback: Type 53](#3-ghost-playback-type-53)
4. [The Ghost volume and the missing mission-VM bridge](#4-the-ghost-volume-and-the-missing-mission-vm-bridge)
5. [Objectives and Ghost navigation: Type 68](#5-objectives-and-ghost-navigation-type-68)
6. [SCOT identity, roster, and opening](#6-scot-identity-roster-and-opening)
7. [SCOT trigger recognition and staged authority](#7-scot-trigger-recognition-and-staged-authority)
8. [Ikora, scenes, gates, and portal presentation](#8-ikora-scenes-gates-and-portal-presentation)
9. [TP and the gate-to-forest crossing](#9-tp-and-the-gate-to-forest-crossing)
10. [Infinite Forest registration, seeding, and generation](#10-infinite-forest-registration-seeding-and-generation)
11. [Towerfall and the Tower Watch cue executor](#11-towerfall-and-the-tower-watch-cue-executor)
12. [The mission graph exports and their limits](#12-the-mission-graph-exports-and-their-limits)
13. [Wire-format and lifetime rules](#13-wire-format-and-lifetime-rules)
14. [Known contradictions and stale comments](#14-known-contradictions-and-stale-comments)
15. [How to investigate the next missing edge](#15-how-to-investigate-the-next-missing-edge)
16. [Source and evidence index](#16-source-and-evidence-index)

## 1. What we know, and what remains missing

Use these evidence labels throughout:

- **Current source:** directly implemented or explicitly represented in the checked-out code. This does not prove the deployed DLL matches it.
- **Historical observation:** a dated handoff records an actual experiment or user-visible result. This does not prove the current tree still reproduces it.
- **Recovered structure:** content identities, schema layouts, or native boundaries preserved in the extraction/evidence code. A structural relationship is not necessarily an executed transition.
- **Inference / unresolved:** a plausible relationship whose actual producer, ordering, or runtime result has not been demonstrated.

The strongest overall findings are:

- Ghost playback has a substantially recovered **consumer**: a Type-53 authority body supplies per-row state; the client checks generation, time, mode, and references; a selected bank row enters the native voice/presentation system.
- The Ghost line selector is in the authored dialogue bank. **It is not serialized as the selector hash in the Type-53 authority record.** Record ordinal selects bank row ordinal.
- The complete native path from the local Ghost volume through an activity-script/VM action to a host-published Type-53 record is **not recovered**.
- The current SCOT implementation publishes specific dialogue/objective state and recognizes two exact Type-30 monitor edges. It also includes a local gate hop and Forest generator experiments.
- Towerfall's opening objective and first Ghost line were reported working on August 27. The wall breach, first Cabal wave, and real encounter-clear/path-unlock sequence were not working.
- The Towerfall export contains **713 nodes but zero explicit successor edges**. It is a useful inventory, not a recovered executable mission graph.
- Infinite Forest **object seeding** and the generator's **random seed** are different mechanisms. Missing platforms can result from missing registration, an uncommitted bubble, absent local authority, or unresolved pieces—not just the random seed.

Primary evidence: [Ghost bridge model][ghost-h], [wire bodies][bodies], [SCOT/Towerfall publisher][snapshot], [safe Towerfall handoff][tower-safe], [edge export][edges].

## 2. The architecture and the different meanings of graph

There are several connected systems, each with its own proof boundary:

```text
Package content
  scenario -> registry/group -> authored slot/definition/schema
                                  |
                                  v
Roster publication -> native object registration -> authority/sense storage
                                  |
              +-------------------+--------------------+
              |                                        |
        client sensing                          host-authoritative state
      Type-30 monitor etc.                       Type-5 publication
              |                                        |
      Type-6 sense packet --------------------> selected state changes
                                                       |
                          +----------------------------+------------------+
                          |             |             |                  |
                    Type 18/35     Type 53 VO      Type 68 HUD       Scene/device/etc.
                          |             |             |                  |
                    mission state   voice queue   objective manager   runtime resolution
                                        |                                |
                                   audible line                    actors/animation/VFX
```

This diagram describes the networked sensor/authority path. The local Type-60 Ghost volume is a separate event path; it has no recovered Type-6 sense exporter. Do not force it into the Type-30 model.

**Roster graph:** which groups and slots exist, which are global, and which belong to bubbles. It enables construction and synchronization.

**Mission execution graph:** conditions, event subscriptions, VM nodes, successors, and action producers. It determines which mission action should happen next.

**Dialogue bank/presentation graph:** bank rows, selectors, voice lookup assets, timing, arbitration, actual start, and teardown.

**Scene graph:** selectors, active-entry references, scene runtime sources, cast members, animation, model building, effects, and retirement.

**Forest generation graph:** sensor parameters, worker state, piece definitions, entry activation, and authority-dependent construction.

Finding nodes in one of these graphs does not recover edges in another. In particular, Type 18 and Type 35 do not contain the entire mission graph, and playing dialogue does not establish that a scene actor exists.

Sources: [general investigation guide][guide], [Ghost evidence model][ghost-h], [Towerfall graph export][manifest].

## 3. Ghost playback: Type 53

### 3.1 Exact SCOT consumer identity

The recovered opening consumer is:

```text
Registry:              0x82FB58B7
Slot:                  type 53 / index 2
Component class:       0x80804F4B
Authority schema:      0x80804F77
Definition:            0x80F47BDA
Dialogue bank:         0x80F1FD07
Bank class:            0x80808D54
Opening record:        0
Opening selector:      0xAD60F465
```

The player opening the Ghost to see navigation, the local Ghost trigger volume, and the Ghost speaking a line are different operations. Their names overlap; their component paths do not.

### 3.2 Decoded memory layout

The decoded Type-53 body is `0x1008` bytes: an eight-byte root reference followed by 128 records of `0x20` bytes each.

On the recovered PC layout:

```text
component + 0x180 : copied authority body / root reference
component + 0x188 : record 0
component + 0x190 : record 0 optional time value
component + 0x198 : record 0 target reference
component + 0x1A0 : record 0 generation, signed i32
component + 0x1A4 : record 0 mode
component + 0x1188: separate processed-generation mirror

record N = component + 0x188 + N * 0x20
generation within a record = +0x18
mode within a record       = +0x1C
```

The authority apply copies the decoded body into the cache. It does **not** clear the separate processed-generation mirror.

### 3.3 Wire layout and exact sizes

The wire form is bit-packed, not a raw copy of the decoded struct:

```text
Root reference: 55 bits

Repeated 128 times:
  mandatory u64                     64 bits
  optional-time presence flag        1 bit
  optional u64, if present           64 bits
  record target reference            55 bits
  biased signed generation           32 bits
  biased mode                         2 bits
```

Thus:

```text
Inactive record = 64 + 1 + 55 + 32 + 2 = 154 bits
Active record carrying time           = 218 bits
All times absent = 55 + 128 * 154      = 19,767 bits
One active record                     = 19,831 bits
Two active records                    = 19,895 bits
```

The current SCOT writer initially activates record 0. At the tunnel entrance it activates both record 0 and record 6. The shared authored-cue writer used by Towerfall activates one selected record at a time.

An active record is written with:

- Mandatory u64 = `0xFFFFFFFFFFFFFFFF`.
- Optional time present, value `1`.
- Canonical absent target reference.
- Decoded generation `1`, wire value `0x80000001`.
- Decoded mode `2`, wire value `3` in the two-bit bias-1 field.

Inactive records omit the optional time and decode to generation 0 / mode 0. Their generation wire value is `0x80000000`, and their mode wire value is `1`.

The nonzero time is essential: the recovered validity path tests whether the stored time is nonzero. Omitting it from an otherwise active record prevents the intended dispatch.

### 3.4 References and why zero-filling failed

A canonical absent reference decodes as:

```text
registry/hash = 0x811C9DC5
type          = -1
index         = -1
```

Its wire representation is:

```text
32-bit hash = 0x811C9DC5
7-bit type  = 0
16-bit index= 0x7FFF
```

The type has bias 1; the index has bias `0x8000`. An all-zero decoded reference can be treated as a real target that must resolve. It then blocks dispatch instead of meaning “no target.” The current writer uses absent references for both the root and each record.

### 3.5 Playback and replay semantics

The recovered play route checks a changed generation, a valid nonzero time, mode 2, and the relevant target/root predicates. It then resolves the selected bank row and submits it to the native presentation system.

Generation comparison is an **equality comparison**, not proof of a monotonically increasing network sequence. Once a row's generation has been processed on the same component instance, reapplying that same generation does not replay it.

The processed mirror is updated **after submission and before actual audible start**. Therefore:

- Successful authority decode does not prove row dispatch.
- Row dispatch does not prove queue acceptance or actual voice start.
- A processed-generation change does not prove the line finished speaking.
- A deadline/arbitration drop is not automatically retried for the same generation.
- Replacement-instance initialization and retail late-join replay policy remain unresolved in the evidence model.

This distinction is directly relevant to Towerfall's opening-to-breach handoff.

### 3.6 Why arming must wait for world arrival

The publisher documents a captured failure: the opening record dispatched around `t=60.7` while loading; its ten-second eligibility expired around `t=70.7` at fade-in. The generation had already been consumed, so the player heard nothing.

Current code arms SCOT dialogue using `mission_seed_armed()`, set when the world reaches `WorldPhase::arrived`. A roster acknowledgement alone is insufficient—it arrived roughly ten seconds early in that experiment.

That latch persists through later transitioning phases and clears at idle. Separately, SCOT opening stage advancement requires the **current** world phase to equal `arrived`.

### 3.7 Known opening voice assets and timing

The Ghost evidence model records these exact opening-row values:

```text
Selector:                   AD60F465
Duration:                   5.444250106811523 seconds
Duration float bits:        40AE374C
Internal ID:                1B0C16D1
Line ID:                    13E8F153
Priority:                   12
Channel:                    166521A5
Root delay:                 1 second
Eligibility:                10 seconds
Primary voice:              80F1FCFD
Primary lookup bank:        80F1FCE8
Primary cue:                B66EEB77
Alternate voice:            FFFFFFFF
Alternate lookup bank:      80F1FCE9
Alternate cue:              166D360E
Speaker:                    EFCD14BE
Wwise bank/event:           22B41355
Wwise action:               245849CB
Wwise sound:                0756D816
```

The current source also identifies the tunnel Ghost line as **record 6**, selector `AE2495AC`, approximately **8.09 seconds**. This compilation does not invent spoken text from those hashes.

### 3.8 Native playback boundaries

All addresses below are RVAs from the pinned PC evidence model:

```text
1009B60 : Type-53 authority apply
1009B88 : decoded-body observation point
1009BF9 : post-copy observation point
100A180 : Type-53 tick / scan
100A267 : generation comparison
100A27B : active-time predicate boundary
100A30A : record-reference gate
100A31A : root-reference gate
100A32A : root predicate
100A337 : record predicate
100A34E : selected-row call
100A353 : post-terminal call
100A35E : post-generation-store observation
10097D0 : selected-row extractor / dispatch
0A38490 : terminal submit wrapper
0A38530 : terminal core
0A3CB80 : voice leaf queue
0A3D3A0 : voice arbitration
0A3D710 : actual voice start
1320380 : timed presentation publication
0A3E2D0 : voice teardown/update
1322AD0 : timed delegate installation callsite
0A3A6D0 : timed delegate setter, known short function
```

An instruction point or a short function is not automatically a valid inline-detour target. The evidence descriptors distinguish these cases.

The PS4 reference establishes generic Type-53 consumer parity, with cache at `+0x178`, record zero at `+0x180`, and processed mirror at `+0x1180`; apply is `0x004A5D10` and tick is `0x004A5D80`. It does not establish SCOT-specific identity or recover the PS4 Type-60 callback family.

Sources for this section: [Ghost header][ghost-h], [Ghost native-boundary implementation][ghost-cpp], [dialogue/Forest probe][probe], [authority body writer][bodies], [snapshot timing logic][snapshot].

## 4. The Ghost volume and the missing mission-VM bridge

### 4.1 Local Type-60 volume

The recovered source volume is:

```text
Registry object/tag: 80F47B5B
Registry key:        BA5F26EF
Slot:                type 60 / index 4
Packed type/index:   0004003C
Authored name hash:  FDF6CB49
Definition:          80F47B4F
Wrapper:             80F47B51
Entity:              80F47B50
Runtime class:       808099C8

Origin: (260.4328613, 242.4115601, 78.4487228)
Extrusion: 50.0
AABB min: (255.3617096, 237.3612366, 78.4487228)
AABB max: (265.5040283, 247.3700867, 128.4487305)
```

The evidence model explicitly states:

- Type 60 has no recovered authority-apply handler.
- Type 60 has no recovered sense-export handler.
- The frozen six-group opening roster does not contain this Type-60 slot.
- Cold-spawn initial-overlap capture is required; the player may begin inside the volume.

Absence from that replicated roster does not prove a local package-owned volume cannot exist. It does mean “wait for the same kind of Type-6 packet as the Ikora approach monitor” is not a justified assumption.

### 4.2 Structural Type-54 destination

```text
Registry object/tag: 80F47BC6
Registry key:        F7A6CE7F
Slot/symbol:         type 54 / index 7
Packed type/index:   00070036
Selector:            AD60F465
```

Type 54 here is a **content/VM symbol**, not a native component to manufacture as another roster object.

The model grades the proposed links separately:

```text
BA5F26EF / 60 / 4
    -> F7A6CE7F / 54 / 7, selector AD60F465
       STRONG INFERENCE; live capture required

F7A6CE7F / 54 / 7
    -> 82FB58B7 / 53 / 2, record 0
       UNKNOWN producer; not recovered
```

The combined bridge is explicitly marked unknown and not promotable to a proven route.

### 4.3 Recovered event/VM observation surfaces

```text
0A70500 : Type-60 create/register
0A6E920 : Type-60 start
0A70300 : Type-60 lifecycle clear
0A70E30 : containment predicate
0A71E20 : membership set
0A71E40 : membership clear
04E7750 : local registration
04E3DA0 : authored-record start
04E1870 : event subscriber registration
04E18A0 : subscriber removal
04E17E0 : event enqueue
04E1820 : enqueue with payload
04E1600 : event dispatch
04E16A0 : dispatch with payload
04E1920 : event drain
17898A0 : activity-script component registration
1789B6E : activity-script event callback
10881A0 : VM runner tick
0A214C0 : VM list/sequence
0A21520 : VM node dispatch
```

Type-60 definition count/data are at `+0x38/+0x40`, with definition records of stride `0x120`. Runtime entry count/data are at `+0x140/+0x150`, with stride `0x20`; membership mask is at entry `+0x08`, and the registration-token offset recorded by the model is `0x62`.

The source-only Ghost bridge module installs no hooks and performs no authority publication or game-memory mutation. Its descriptors, bounded evidence records, hashing, and tests are infrastructure for proving a route. Their existence does not mean a live bridge capture has happened.

Still missing according to that model:

- Exact Ghost local-event ordinal.
- Producer instruction that turns the VM action into the selected Type-53 record.
- Retail host publisher and its RVA.
- Type-53 completion feedback into mission state.
- Dialogue sense export and mission-result consumer.
- Retail late-join/replay policy.

The frozen-opening callback-count field is zero. Treat that as evidence about that captured configuration, not proof that all retail missions lack activity-script callbacks.

Sources: [Ghost evidence model][ghost-h], [native descriptors][ghost-cpp].

## 5. Objectives and Ghost navigation: Type 68

SCOT's shared cue registry is `82FB58B7`:

- Type 68 / index 0: objective/directive tracking.
- Type 11: music-related state in the opening roster.
- Type 53 / index 2: dialogue.

The Type-68 authority schema is `80804F67`. Its recovered wire body is **4,802 bits**:

```text
two references                 2 * 55 bits
three rotating records         3 * 1,563 bits
ring selector                  3 bits
total                          4,802 bits
```

The native records have stride `0xF8`; the probe identifies records at component `+0x190` and the ring selector at `+0x478`. Authority apply is RVA `1009C00`.

Current SCOT publication uses active record 0 and marks the other two records absent:

- Opening event `C252E306`: “Hunt down and destroy Panoptes.”
- Opening discriminator 0 produces the recorded HUD identity `32678D66`.
- Forest event `1EBF4621`: “Destroy Panoptes, the Infinite Mind / Avert the future where the Vex win.”
- Lifecycle 0 is active/installable; lifecycle -1 represents absent records.
- The ring selector decodes to 0.

Time/progress fields use the engine's all-ones no-value timestamps. Using zero as an end time produced an unwanted `0:00` countdown.

The Forest banner switches on the persistent entrance latch, or on the direct-launch region check `64 <= region <= 112`. This differs from the generator's direct-region window, which ends at 104. The code documents that a walked boundary can change the client's local region while server membership still advertises 120, so relying only on server region prevented the banner from changing.

Historical observation: restoring the exact SCOT roster, especially `82FB58B7`, restored the objective marker when opening Ghost. That is evidence for the navigation/cue topology, independently of scene construction or complete mission execution.

Sources: [body writer][bodies], [publisher][snapshot], [August 23 script handoff][omega-script], [Type-53/68 evidence support][capture-53-68].

## 6. SCOT identity, roster, and opening

SCOT is the internal package name `mission_scot`, called **Omega** in the project notes.

Recorded launch identity:

```text
Activity index:        299
Scenario:              80F47522
Opening bubble:        15, Lighthouse
Opening name hash:     A83A9175
Opening region/slice:  120
Opening spawn set:     4AB3287A
Absent hash sentinel:  811C9DC5
```

The measured six-group opening roster is:

```text
4786C0E0 : player/runtime components, including 18 and 35
29D7B029 : navigation/type-19 group
82FB58B7 : directive, music, dialogue
BA5F26EF : Lighthouse transport/mission group
D00142CF : Ikora, Scene, portal devices, monitors
F7A6CE7F : authored mission/dialogue-related group
```

The exported opening inventory reports **6/6 groups, 57/57 slots, and raw schemas for all 57 slots**. Three groups are top-level; three are bubble-local to bubble 15 in the measured baseline.

The current runtime also amends the Forest generator group `2763EC97`. Thus the six-group export is the opening inventory; it is not evidence that the amended live roster must contain only six groups.

Important opening content identities:

```text
Opening registry object:       80F47BB2
Ikora approach volume def:     80F47B6C
Squad/spawner def:             80F47B70
Scene authority object:        80F47B73
Gate controller def:           80F47BA0
Engagement sensor def:         80F47BA3
Vignette point:                80F47BA6
Dialogue point:                80F47BA9
Ikora entity:                  80EC0F27
Ikora presentation component:  80EC13A2
```

The intended opening order represented in the current publisher is arrival, Ghost pre-roll/objective, approach to Ikora, vignette/scene, handoff, portal interaction, and Forest continuation. This is not a claim that all those transitions currently have working producers.

Sources: [opening inventory][omega-inventory], [roster extraction][roster-groups], [historical reconstruction][omega-complete], [current publisher][snapshot].

## 7. SCOT trigger recognition and staged authority

### 7.1 Two exact Type-30 entered edges

The host recognizes two distinct objects:

```text
Ikora/opening approach: D00142CF / type 30 / index 20
Forest gate entrance:  D00142CF / type 30 / index 24
```

Both recovered predicates require:

```text
one group, one object
group registry = D00142CF
group body     = 156 bits
object body    = 100 bits
bodyFirst      = F000000030000000
bodySecond     = 0000000000000002
bodyThird      = 0
```

The object type/index distinguishes the two. Session/destination binding, matching epoch, successful parse, and roster readiness are also part of accepting the event. A nearby coordinate or a matching low index alone is insufficient.

On an accepted first opening edge, the route sets `omegaOpeningTriggered` and wakes the keepalive publisher. On an accepted first entrance edge, it sets `omegaForestEntranceTriggered`, wakes the publisher, and queues `request_omega_gate_hop()`.

Duplicate edges are classified as duplicates. These Type-6 packets do not become ad hoc request/reply commands; the normal publication path owns the outgoing update.

### 7.2 Initial sense reports and readiness

The route also recognizes recovered 235-/236-byte bootstrap forms. The expected six sense objects are:

```text
BA5F26EF / 23 / 1
BA5F26EF / 70 / 2
D00142CF / 1  / 0
D00142CF / 43 / 1
D00142CF / 23 / 16
D00142CF / 70 / 17
```

The readiness model includes the exact opening roster topology and supports the appended generator acknowledgement on bubble 11. Earlier handoffs described these packets as incompletely parsed; the current code has more specific topology and body validation.

A captured 140-bit `D00142CF/43/1` Scene sense body is preserved as observation evidence. Its mere appearance is explicitly **not** used as a progression predicate.

### 7.3 Stage numbers are not script-state numbers

The publisher's internal stage enum is:

```text
0 None
1 Baseline
2 Scene
3 Ready
4 Triggered
5 Spawner       (retired; skipped for compatibility)
6 Portal
7 Completed
8 Settled
9 ForestTransition
10 ForestSettled (reserved successor)
```

The corresponding selected Type-18 script scalars are:

```text
Baseline publication        -> 1
Triggered publication       -> 2
Completed publication       -> 3
ForestTransition publication-> 4
```

Do not equate stage 4 with script state 4.

Current opening eligibility requires SCOT, initialized mission-authority runtime, roster readiness, current world phase `arrived`, and the mission-seed latch.

The implemented transition logic then:

1. Holds at None until the approach monitor has latched.
2. Publishes Baseline and initializes selected mission state.
3. Advances through Scene and Ready seeding stages.
4. Requires `omegaSceneHandoffArmed` before Triggered/state 2.
5. Requires `omegaSceneCompleted` before post-scene Portal/Completed handling.
6. Uses entrance and transport flags to select later state 3/state 4 publications.
7. Retains state 4 until a later observed mission event provides a successor.

### 7.4 Current reachability limitation

**Source inspection finding:** searches across `Dawn/src` found declarations, reads, and resets to false for `omegaSceneHandoffArmed` and `omegaSceneCompleted`, but no producer assigning either to true.

Consequently, the existence of the later stage branches is not evidence that the checked-out implementation can reach them through ordinary execution. Historical notes describe scene retirement/handoff behavior from earlier experiments; those observations must not be promoted into a claim that this tree has the same connected path.

Likewise, `host_action=start_opening_scene` in a log is a route label for a latched input. It is not proof that a native Scene runtime was constructed or completed.

Sources: [sense route][route], [publisher][snapshot], [publication commit][push], [world latches][world].

## 8. Ikora, scenes, gates, and portal presentation

### 8.1 Scene identities and historical observations

The August 22 reconstruction distinguishes:

```text
Opening master timeline: 80FCCE87
User-tested Scene 1 cast: 80EC0F0E
User-tested Scene 2 cast: 80EC0FA8
User-tested Scene 3 cast: 80EC0FA6
Open portal scene:       80C51CBE
Earlier portal reference:80C51CBD
Purple effect refs:      80B9FDBE, 80C220EB
```

The master timeline and the cast handles are not interchangeable.

Historical A/B experiments found that disabling all three cast scenes left Ikora dialogue/audio playing. The actor, model, animation, events, and audio paths therefore require separate validation. Earlier work kept a problematic Scene-2 actor/event path alive while suppressing its visible model to preserve authored effects; that was a case-specific workaround, not a general mission graph implementation.

### 8.2 Authority identities

```text
D00142CF / 43 / 1    : opening Scene controller
Scene selector      : 80EC0F96
BA5F26EF / 4 / 0     : authored Lighthouse portal transport component
D00142CF / 4 / 2..4  : portal visual components
D00142CF / 4 / 7..15 : nine gateway push/barrier panels
BA5F26EF / 23 / 1    : gate/interior controller
D00142CF / 23 / 16   : mission gate controller
D00142CF / 70 / 17   : engagement sensor
```

The experimental Type-4 body is 253 bits under schema `8080992F`. The Type-23 gate body is 147 bits: three `{float32, biased-int16, bool}` records under `80804F48`.

### 8.3 The barrier is not the starry portal

The gate-authority readiness constant is currently false. Its comment records a user-confirmed regression: setting gate device channel 0 to zero removed the always-on starry portal presentation. The blocking Vex wall is an additive `vex_wall` device layer in front of it.

Therefore “turn the portal on/off” and “dissolve the barrier” are separate actions. Publishing the wrong gate channel state can remove the correct backdrop without fixing the wall.

Related native boundaries include gate apply `10699C0`, device channel setters `DF6BD0/DF7120`, and device configure `DF5070`. The configure hook must preserve its native return value; the source records that dropping the return corrupted configure success and broke presentation.

### 8.4 SCOT Scene-body inconsistency

The current header sets `kOmegaSceneAuthorityBodyReady = true`, while its adjacent comment says the old 37-bit Scene prefix is invalid against schema `8080626B`'s 74-bit minimum. The SCOT inactive Scene writer still emits the 37-bit prefix.

This is an unresolved source inconsistency. The recovered **129-bit Towerfall active-entry body** below does not automatically validate SCOT's different 37-bit path. Recheck the exact decoded SCOT body and the consumer contract before calling this codec solved.

Sources: [historical scene work][omega-complete], [body readiness and stage definitions][auth-h], [body writer][bodies], [probe][probe].

## 9. TP and the gate-to-forest crossing

### 9.1 The current route: local body hop, then walking/streaming

The checked-out entrance handler calls `request_omega_gate_hop()`. The frame consumer queues an absolute move through the teleport module:

```text
Shared tunnel spawn set: B34BD817
Current target:          (-1357.1, 1213.2, -45.8)
Side:                    Lighthouse-side tunnel point
```

The spawn set has two authored trios, one on each side:

- Forest-side trio around `y = 1144.6`.
- Lighthouse-side trio around `y = 1213.2`.

The code deliberately uses the Lighthouse side. The player then walks across the native region-volume/streaming boundary into `infinite_forest_d`. A recorded experiment that landed directly on the Forest side changed the region immediately but stranded the player in unloaded space.

The actual code uses the fixed Lighthouse coordinate; comments describing nearest-point resolution should not be mistaken for its current implementation.

`request_absolute_move()` queues work for the physics tick that owns the local player. It applies regardless of the overlay's ordinary teleport toggle. A queued move alone does not prove successful streaming or correct server membership.

Facing is left unchanged. The source documents that writing derived camera-forward copies snapped back when released, and the true aim/yaw storage was not recovered for this path. The player may need to turn roughly 90 degrees right in the tunnel.

### 9.2 Retained native transition experiment

The probe also contains a separate pending-request path:

```text
Target slice:       88
Spawn hash:         4E5FD117, ap_inside_forest
Precheck:           E2E720
Request wrapper:    E2E7E0
Native promotion:   E25A30
Native type-7 start:E2B120
```

It waits 45 frame polls, checks the manager's pending-request condition, verifies entry-point prefixes, quiesces Omega authority, then requests the transition on the game thread. The delay is a compatibility experiment, not an authored trigger duration.

Quiescing is necessary in that experiment because authority applied into a slice being torn down caused an index-heap/double-free failure.

**Current call-graph finding:** `request_omega_forest_transition()` exists, and the consumer exists, but the source search found no caller beyond its declaration/definition. The ordinary entrance handler currently queues the local gate hop instead.

The native world-transition operation called “type 7” here should not be conflated with a BAP service number merely because both use the number 7.

### 9.3 Retained next-launch arrival override

A separate latch can survive activity teardown and be consumed by the next SCOT launch. The session-creation code then applies:

```text
arrival bubble override = 11
slice-set override      = 88
spawn-set override      = B34BD817
```

It runs after the forced-destination override, since forcing slice 120 would otherwise defeat the Forest arrival choice.

**Current call-graph finding:** the latch setter `note_omega_forest_arrival_pending()` has no caller in the searched source tree. This is retained machinery, not the current automatic effect of entering the gate.

### 9.4 Region and transport proof

In this extracted topology, a bubble's base region is its ordinal multiplied by eight. Lighthouse is bubble 15 / base region 120; Forest D is bubble 11 / base region 88. Bubble ordinal, region, and selected slice set remain separate fields.

The entrance monitor proves that the player entered the monitor. The queued hop proves a request. Local region change proves local traversal. Server transport/membership confirmation is another boundary. The banner/generator latch workaround exists because these were not all advancing together.

Sources: [sense route][route], [transport consumer][probe], [teleport interface][teleport], [world latches][world], [next-launch override][arrival-route], [topology inventory][omega-inventory].

## 10. Infinite Forest registration, seeding, and generation

### 10.1 Three separate requirements

1. **Registration:** the authored generator sensor and related objects exist in the roster/native scope.
2. **Synchronization seeding:** the bubble's required sync records commit, allowing the native construction sweep.
3. **Generation:** the worker has usable parameters, enable state, authority, and piece definitions and can activate a layout.

The number `9001` belongs to the third step. It cannot compensate for a missing worker or an unseeded bubble.

### 10.2 Missing group amendment

The runtime amends this extracted group into SCOT:

```text
Registry:          2763EC97
Registry object:   80F47539
Roster index hint: 1070, searched within +/-64 and checked by key
Bubble mask:       1 << 11
Slots:             70/0, 37/1, 30/2, 57/6
Generator sensor:  2763EC97 / 37 / 1
```

The original opening scenario lists reached only the six opening groups. Without the amendment, the Type-37 component was not constructed and Forest platforms did not build.

The group is kept in a stable published roster across the crossing so changing topology does not destroy/recreate the mission objects unnecessarily.

### 10.3 Why object blocks must keep arriving

The encoder documents that the client's object-record processor can rewind/ignore a block when that object's bubble is not current. Sending all Forest records once while the player is in Lighthouse does not prove the Forest records were seeded.

`write_forest_generator_group()` therefore continues emitting the group. It forces the auth flag on every extracted slot because the extracted flags omitted some slots known to have sync-pool objects, including `70/0` and `30/2`.

The intended empty block is effectively `{reset=1, payload-present=0}`. It supplies synchronization without inventing an unproven authority payload.

### 10.4 The generator wire body is parked

The current writer contains a candidate **11,350-bit** body for authority schema `80805007`, but `kOmegaForestGeneratorBodyReady` is false.

Its proposed structure is:

```text
80805007 -> 80805008
  two 8080500B records, 5,275 bits each
  one 80805009 tail, 800 bits
total = 11,350 bits
```

The candidate uses one-bit booleans and fully serialized fixed arrays. Each `8080500B` includes a fixed 100-element tile array of three biased 16-bit values per tile, even when meaningful tile count is zero. The tail includes a u32 and 32-/64-byte arrays.

The source parks this body because an undecodable body leaves a slot unseeded and prevents the whole bubble seed commit. Comments report that the worker can use authored defaults, including seed 9001, without a valid host-authored generator body. The current runtime also applies explicit ignition and seed-check workarounds, so it must not be described as an entirely untouched native-default route.

### 10.5 Sensor and worker are different objects

Sensor:

```text
kind                  80804EF6
slot                  2763EC97 / 37 / 1
create RVA            103E450
authority apply RVA   103E8E0
sense RVA             103D820
authority record      instance + 0x180
sense record          instance + 0x734
record size           0x5B4 bytes
```

Worker:

```text
kind                  80805017
definition class      80804FED
map-object class      808099D6
source package IDs    03A6 / 03A7, per-segment workers
create RVA            FFE820
tick RVA              10059A0
entry activation RVA  10020A0
tick key              128CC0BD4
state byte            instance + 0x9BC
entry count           instance + 0x924
entry table relative  instance + 0x858
entry stride          0x38
entry state byte      entry + 0x29
entry palette         entry + 0x32
```

The worker resolves a Type-37 sensor in scope through the recovered path around `103E700`. It can consume host-committed activation state or self-generate when its authority gate allows it. The model records seed echo at record `+0x550`, with 32/64 activation fields starting at `+0x554/+0x574`.

The host logs Forest sense revision, active-32 and active-64 values, and incident correlation. Observing these is not equivalent to implementing the full host generation policy.

### 10.6 Current local tuner

The Forest overlay operates a shared atomic state read by the worker hook. These controls are local experiments, not recovered retail network commands:

- Generator enable/ignition: default true.
- Optional seed write: displayed default 9001; write flag initially false.
- Optional mode write: displayed default 2; UI range 0–7; write flag initially false.
- Four optional groups: byte `a`, byte `b`, weight, active flag. Default weight 1; group write flags false.
- Optional float pair: defaults 0.12 and 0.16.
- Optional int pair: defaults -1 and -1.
- Per-worker entry activation requests for up to four worker slots.
- Live sensor, apply-count, and per-worker entry-count displays.

The implementation writes selected fields every 30 calls to the tuner application function. This refresh counters later authority overwrites while the worker's change detection decides whether to rebuild.

Current implemented field map:

```text
record + 0x00 : seed, presence bit 0
record + 0x04 : mode, presence bit 1
record + 0x08 : four group records, stride 9, presence bit 2
  group + 0 / +1 : a / b
  group + 4      : float weight
  group + 8      : active
record + 0x2C : presence mask
record + 0x2D : enable, presence bit 3
record + 0x30 / +0x34 : float pair
record + 0x38 / +0x3C : int pair
```

These are the offsets the current tuner uses, not a claim that all field meanings are understood. The panel's “density” labels and mode choices remain experimental semantics.

**Source caveat:** disabling the UI enable flag is not a reliable shutdown command in this tree. The tuner only sets enable when true, and the worker hook separately forces enable/presence when it observes them absent or zero. It does not implement a symmetric clear path.

Comments about reloading `forest_tuner.txt` every ~600 ticks are stale; the shown implementation reads shared UI state. The panel says settings are nonpersistent between launches.

### 10.7 Manual piece activation and missing platforms

Manual activation calls the native per-entry routine on the worker tick thread. The implementation first verifies the index is below the worker entry count and that the entry exists with nonzero initialized state.

Recorded failures explain those checks:

- An out-of-range index around 20 corrupted memory/crashed.
- Activating skipped/uninitialized entry 18 froze the game.

The source also records a more specific cause for missing first/last platforms: the build path at `FF8300` skips network-replicated pieces when self-authority is false. The relevant definition flag is bit 4 at `def+0x98`; filtering passes through `14E4720`, with self-authority through `FFF500 -> 4E7F70` and bubble/registry gates. Probes also query start-active at `FFF4A0`.

A different random seed is not established as the fix for those missing replicated pieces.

### 10.8 Active runtime workarounds

The broadly named `omega_dialogue_dispatch_probe.cpp` contains more than observation:

- Local gate-hop requests.
- Tuner writes to generator authority memory.
- Enable ignition.
- Manual entry activation.
- A seed-check override.
- An index-unregister guard associated with teardown failures.

The seed-check hook calls the original and, if it fails, can return success when the pending mask is nonzero and confined to `0x7F00` (bubble bits 8–14). This can allow construction to proceed, but a forced verdict is not proof that every native seed dependency was satisfied naturally.

The map factory at `56D9B0` is already hooked by the Ikora-origin probe. A second competing detour in this probe previously failed prefix verification and prevented other hooks, including the teardown guard, from installing. That installation dependency was linked to an August 27 tunnel freeze.

Sources: [roster amendment][catalog], [encoder][encoder], [body writer][bodies], [combined probe][probe], [tuner state][tuner], [Forest panel][forest-ui].

## 11. Towerfall and the Tower Watch cue executor

### 11.1 Launch and working boundary

Historical August 27 results:

```text
Mission:            Homecoming
Internal package:   mission_towerfall
Activity:           266
Native launch donor:282 / Chosen
Opening bubble:     9
Opening slice set:  72
```

The Chosen route supplies a native prelaunch path, with selected destination rewritten to Towerfall before publication. Launch success is separate from mission execution.

Recovered Towerfall IDs in the bootstrap code:

```text
Investment hash:          62D85FB3
Package-definition hash:  9ACCB518
Activity tag:             80B500AC
Launch descriptor:        80FDB97F
```

The safe handoff reports working roster decode, manager registration/component dispatch, opening objective, and Ghost record 0. It explicitly lists the wall breach, first Cabal combat wave, encounter-clear detection in actual combat, path unlock, and universal cue executor as not working.

### 11.2 Three-beat manifest

Current cue roots:

```text
Mission runtime registry: 4786C0E0
Root cue registry:        664128F4
Tower Watch local:        9D8076E4
Directive slot:           68/0
Dialogue slot:            53/2
```

The small authored manifest defines:

1. **Opening** (`opening_objective_and_ghost`): directive `4FCECAB6`, dialogue record 0, script state 1.
2. **Breach** (`wall_breach_and_first_cabal`): directive `432D2C95`, dialogue record 1, script state 2.
3. **Path unlocked** (`encounter_clear_and_path_unlock`): directive `432D2C96`, dialogue record 2, script state 3.

These names describe intended beats. A name mentioning Cabal or a path unlock does not establish that those world actions currently occur.

The publisher sends one undelivered beat at a time. Publication commits `towerWatchPublishedStage` in the delivery path; it records the opening commit tick and wakes the next eligible beat. Later keepalives preserve existing authority state instead of resetting it.

### 11.3 Opening-to-breach handoff is a submission acknowledgement

The dialogue scan hook calls the original scan, then checks record 0 for:

```text
time != 0
generation == 1
mode == 2
processed before == 0
processed after  == 1
```

It sets `tower_watch_opening_dialogue_processed()`. Once the opening beat is published, the host uses this latch to arm the breach beat.

Some handoff prose calls this “dialogue completion.” The recovered consumer semantics support **submission/processed acknowledgement**, not “the line has audibly finished.” Actual-start and teardown must be observed separately before treating this as a retail completion edge.

### 11.4 Trigger candidates and encounter logic

The old startup choreography candidate `9D8076E4 / type 3 / index 11`, body hash `6018351690B58115`, is explicitly rejected as a wall trigger.

A movement-sensitive candidate is `9027B6A1 / type 30 / index 3`. Its logging code is inside an iteration that first skips objects whose registry is not `9D8076E4`. In the inspected source, that different-registry candidate cannot reach the inner branch. Its defined identity is useful evidence, but the current branch is not a working detector.

The engagement candidate is `9D8076E4 / type 70 / index 126`:

1. Record the first body hash as the baseline.
2. After breach is seen, a transition away from baseline marks engagement active.
3. A later transition back to baseline marks `towerWatchEncounterChanged` / encounter clear.
4. The next eligible publication selects the third beat.

This is implemented hash-transition logic. Its semantic equivalence to real combat completion is still unverified because the first combat wave itself was not functioning in the recorded safe run.

### 11.5 Recovered breach Scene body and the unresolved native source

```text
Scene controller: 9D8076E4 / type 43 / index 5
Schema:           8080626B
Selector:         80B82771
Active entry:     57318E3B / type 2 / index 1
Entry count:      1
Word count:       0
Body size:        129 bits
```

The generalized schema shape is:

```text
32-bit selector
5-bit active-entry count
55 bits per active reference
5-bit word count
32 bits per word
32-bit mode

74 base bits + 55 * entries + 32 * words
```

Two important codec corrections were recovered:

- Counts are **5 + 5 bits**, not the earlier 4 + 6 split. The old split doubled decoded entry count and shifted the registry.
- Selector is biased signed i32. To decode `80B82771`, the writer emits `00B82771` after adding `80000000` modulo 2^32.

The corrected body decoded, but its native active-entry runtime source did not resolve. The safe handoff traces:

```text
B3F620 Scene update
  -> +501AD0 entry resolver returns false
  -> +B3F6CB supplies null runtime source
  -> +4E83B0 -> +4E3410
  -> +A92AE0 adds 0x2F0 to null
  -> +A93070 produces iterator pointer 0x314
  -> +A9309B faults reading [0x314]
```

Current `kPublishBreachSceneAuthority = false` prevents publication of the active Scene entry. The breach beat can still publish cue/director/script state, with `scene=0 scene_blocked_unsafe=1`.

This is now a runtime binding/producer problem, not simply an unresolved bit-width problem. The observation path includes Scene update `B3F620`, resolver `501AD0`, and caller `B3F678`. No recovered valid producer contract currently justifies turning the Scene publication gate back on.

Sources: [Tower Watch manifest][tower-beats], [route][route], [snapshot][snapshot], [commit path][push], [dialogue scan][probe], [safe handoff][tower-safe], [bootstrap][bootstrap].

## 12. The mission graph exports and their limits

### 12.1 Towerfall inventory and sense mapping

The saved manifest reports:

- 17 groups.
- 713 named nodes / structural containment links.
- 89 trigger-capable nodes.
- 201 action-capable nodes.
- 3,738 co-resident trigger/action candidate pairs.
- **0 explicit successor edges.**

The sense mapping reports 98 observed objects, all 98 mapped, no missing/ambiguous objects, and `complete=true`. Some object widths are explicitly marked inferred; for example, the shown Type-2 mappings use 46-bit inferred widths. “All mapped” means the captured objects were assigned identities, not that every schema or successor is proven.

The source extraction summary contains 299 descriptor nodes, 15 schema classes, 35 readable descriptor-tag references, one decoded cue table, 16 cue records, and 17 cue values. It reports zero candidate name edges and no recovered direct/schema/reference registry matches in that extraction.

Do not confuse **713 named nodes**, **299 descriptor nodes**, **98 captured sense objects**, and **three executor beats**. They measure different things.

### 12.2 Decoded Towerfall cue table

The table is tag `80B508FE`, class `80804F72`. Its 16 keys, in package order, are:

```text
 0  4FCECAB6
 1  432D2C95
 2  432D2C96
 3  B85090A0
 4  E5ED7DB0
 5  41222C61
 6  BB13D094
 7  23716DE6
 8  D56AE99D
 9  E68735ED
10  5DC9D705
11  F0D48F30
12  D15B9A42
13  DA1CA185
14  57395492
15  E84A26AA
```

The export retains their value hashes, flags, absent fields, and match arrays. Record 11 has two values, accounting for the 17 values across 16 records. The small Tower Watch manifest uses the first three directive keys; package ordering alone does not prove the entire mission's execution order or identify all dialogue-bank rows.

### 12.3 What would count as a real successor edge

A defensible edge needs a recovered typed reference or an observed producer chain connecting a specific input to a specific output, in the same activity/epoch/lifetime context.

Sharing a group, matching an index, appearing near each other in a dump, or having a plausible name establishes a candidate—not causality. The export's verdict explicitly says opening successors are not recoverable from its decoded package fields yet.

Sources: [manifest][manifest], [sense mapping][mapping], [cue sources][sources-json], [edge verdict][edges].

## 13. Wire-format and lifetime rules

These rules explain many of the apparent mission/dialogue failures:

- Type 5 is the activity authority/roster transport; Type 6 carries client sense updates. Type 53/68/30/43 are component slot types, not those packet numbers.
- Exact registry, type, and real slot index form an object identity. Array ordinal is not necessarily the slot index.
- Patch epoch must match. The snapshot echoes the epoch expected by the client; a wrong epoch can skip phase-2 processing.
- Phase-2 object bodies have no convenient resynchronization point. One shifted bit can corrupt all later blocks.
- Use each signed field's bias and optional-field presence semantics. Zero-filled bytes are not automatically a neutral decoded object.
- Fixed arrays may serialize fully even when a meaningful-count field is zero. Reflection metadata that describes a bool with width zero does not mean no wire bit is emitted.
- Roster registration, object seeding, authority apply, and native behavior are separate states.
- Changing `stateSequence` changes object lifetime and tears down/rebuilds roster-owned objects. It is not a dialogue generation or a harmless auth-body revision.
- Keepalives should preserve committed mission authority instead of continually overwriting it with startup state.
- Avoid active payloads for objects whose complete schema/consumer contract is unproven. Empty synchronization bodies can be useful where invented authority bodies block seeding.

Selected known widths:

```text
Shared mission state 808099C4: 353 bits
  bool + five u64 + u32
Type-35 director 808099BF:    359 bits
  two bools + two 2-bit biased enums + shared state
Type-18 script 80809919:      386 bits
  shared state + bool + biased i32
Type-53 dialogue:            19,831 bits with one active timed row
Type-68 directive:           4,802 bits
Active one-entry Scene:      129 bits under 8080626B
Portal presentation body:    253 bits
Gate link-state body:        147 bits
Forest generator candidate:  11,350 bits, currently parked
```

The current `kMissionBodies` registry is empty. Its comment records that earlier schema tooling started the descriptor array `0x18` bytes too early, producing short/misbiased monitor and engagement bodies. Old neutral-body lists should not be copied back as if validated.

Sources: [authority header][auth-h], [body writer][bodies], [encoder][encoder], [general guide][guide].

## 14. Known contradictions and stale comments

These distinctions matter when someone resumes this work:

1. **August 23 “host does nothing” versus current route.** That handoff accurately described its baseline. Current code latches the opening and Forest monitor edges and queues the gate hop.
2. **Historical complete-opening claims versus current reachability.** The August 22 reconstruction records successful parts of an earlier experimental sequence. Current handoff/completion latches have no producer setting them true in the searched source.
3. **19,767-bit dialogue comments versus active writer.** That is the all-times-absent size. One active timed record is 19,831 bits; two are 19,895.
4. **“Zero root reference” / “time omitted” comments versus code.** The actual dialogue writer uses canonical absent root/record references and writes nonzero time for active rows.
5. **Roster acknowledgement versus world arrival.** Current publisher comments explicitly reject roster acknowledgement as the playback-ready signal.
6. **Towerfall “dialogue completion.”** Current handoff observes the processed mirror after submission, not confirmed audible completion.
7. **SCOT Scene readiness flag.** The flag is true while comments and the 37-bit writer conflict with the recovered 74-bit minimum. Treat it as unresolved.
8. **Generator-ready comments versus compile-time gate.** A generator snapshot flag can be armed while the candidate wire body remains disabled.
9. **“Observation-only” probe header versus whole file.** The dialogue consumer observation is only part of a file that now also contains runtime mutations and guards.
10. **Forest text-file comments versus UI implementation.** Current field application reads shared overlay state and is throttled every 30 calls.
11. **Forest enable checkbox versus code.** The implementation sets/repairs enable and lacks a corresponding reliable disable path.
12. **Nearest tunnel point versus hardcoded target.** The current gate consumer queues the explicit Lighthouse coordinate.
13. **Retained native transition and next-launch latches versus actual callers.** Their setters have no caller in the searched tree; the local hop is the connected entrance route.
14. **Towerfall wall-approach candidate logging.** Its registry check is unreachable under the enclosing different-registry filter.
15. **Export completeness versus mission completeness.** Complete object mapping does not provide successors, runtime producers, or combat behavior.

These findings are documentation of the inspected tree; no fixes were applied as part of this task.

## 15. How to investigate the next missing edge

### 15.1 Ghost producer capture

Capture cold spawn and initial overlap, then correlate Type-60 registration, membership, exact local event and payload, activity-script callback, VM node, and Type-53 mutation/publication. Record activity, epoch, object lifetime, registry/type/index, record ordinal, generation before/after, and exact build identity.

The missing proof is not another successful direct write of record 0. It is the native producer that chooses that row because the authored Ghost action executed.

### 15.2 Dialogue consumer diagnosis

Check boundaries in order:

1. Was the correct cue registry/slot constructed?
2. Did Type 5 reach the expected schema and decode to the expected body length?
3. Did the cache receive the intended row/time/generation/mode and absent references?
4. Did the scan see a generation different from its mirror?
5. Did reference/predicate gates pass?
6. Which bank row and selector reached terminal submit?
7. Was a voice actually started before its deadline?
8. Did it end/teardown, and did any mission callback consume that result?

If the mirror changed but no voice started, investigate the queue/arbitration/deadline path rather than replaying the same generation and assuming another authority apply will fix it.

### 15.3 SCOT opening and Forest diagnosis

For opening, verify roster, arrival, exact `30/20` acceptance, actual published script state, native Scene resolution, and a real producer for handoff/completion.

For the gate, verify exact `30/24` acceptance, local hop application, the walked streaming boundary, Forest group presence, bubble seed result, sensor construction, worker construction, ignition, authority verdict, initialized entries, and actual visible/collidable pieces.

Log `seed_forced` separately from natural seed success. Record the tuner configuration and which write toggles were enabled before attributing a layout to a random seed.

### 15.4 Towerfall next boundary

Recover why the Scene entry resolver cannot turn `57318E3B/2/1` into the required native runtime source. Correct serialization already reached that boundary in the recorded experiment. Keep cue playback, Scene runtime resolution, actor creation, and actual combat results distinct.

Then establish a real encounter-active/clear signal and a typed successor before expanding the three-beat manifest into a general executor.

### 15.5 Useful log searches

Choose the log that belongs to the exact run/DLL under investigation. Existing notes use both workspace and deployed `bin/x64` locations; do not silently mix runs.

```powershell
$dialogueLog = 'C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log'
rg -n 'ev=omega_dialogue|ev=omega_arm|dialogue_handoff' $dialogueLog
rg -n 'omega_opening_observer|omega_opening_authority|omega_forest_entrance' $dialogueLog
rg -n 'ev=omega_gate|ev=forest|seed_forced|authority_probe|entry_skip' $dialogueLog
rg -n 'tower_watch_executor|scene_entry_runtime_resolve|scene_blocked_unsafe' $dialogueLog
rg -n 'sensor_sense_update|sensor_sense_capture|object_block|body_bits|patch_epoch' $dialogueLog
```

## 16. Source and evidence index

The links below resolve to the workspace used for this compilation. The symbol names and filenames remain useful if the document is copied elsewhere.

- [Ghost Type-60/VM/Type-53 evidence model][ghost-h]: exact identities, evidence grading, missing producers, replay limitations, cross-platform boundaries.
- [Ghost boundary implementation][ghost-cpp]: artifact verification and native observation descriptors.
- [Type-53/68 capture support][capture-53-68]: artifact/cohort verification and dialogue/directive observation surfaces.
- [Authority body writer][bodies]: actual dialogue, directive, Scene, portal, and parked Forest serialization.
- [Authority header][auth-h]: snapshot fields, stage constants, readiness gates, object lifetime contract.
- [Authority encoder][encoder]: selected cue publication and continued Forest object seeding.
- [Activity sense route][route]: exact SCOT monitor matches, Towerfall observations, Forest sense correlation.
- [Roster snapshot publisher][snapshot]: arrival timing, active records, objective selection, mission stages, Tower Watch beats.
- [Roster publication commit][push]: publication-correlated latches and Tower Watch stage commits.
- [World/arrival latches][world]: per-world state, persistent seed latch, local hop, retained next-launch state.
- [Session arrival override][arrival-route]: retained Forest launch overrides.
- [Combined dialogue/Forest/transport probe][probe]: native consumer observations, local hop, generator controls, seed override, guards.
- [Teleport runtime interface][teleport]: queued absolute-body movement and local-player physics ownership.
- [Forest roster amendment][catalog]: generator group lookup and bubble mask.
- [Forest tuner state][tuner] and [Forest UI][forest-ui]: actual controls, defaults, write flags, worker entry requests.
- [Tower Watch three-beat manifest][tower-beats]: cue keys and disabled breach Scene publication.
- [Towerfall bootstrap][bootstrap]: destination identities and native launch-route observation/mutation code.
- [SCOT roster extraction][roster-groups]: measured opening topology.
- [SCOT authored inventory][omega-inventory]: all 57 opening slots and topology/schema inventory.
- [Towerfall node manifest][manifest]: 17 groups and 713 nodes.
- [Towerfall sense mapping][mapping]: 98 captured object mappings, including inferred-width markers.
- [Towerfall cue-source export][sources-json]: descriptor references and complete 16-record cue table.
- [Towerfall edge verdict][edges]: why co-residence does not recover successors.
- [August 27 safe Towerfall handoff][tower-safe]: observed opening success and exact unresolved Scene crash chain.
- [August 23 SCOT script handoff][omega-script]: earlier six-group/marker-working baseline and exact Type-6 evidence.
- [August 22 reconstruction][omega-complete]: historical Ikora/scene/VFX/portal experiments; do not assume all patches persist.
- [General investigation guide][guide]: broader networking, authority, lifetime, and proof-boundary context.

[ghost-h]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/opening_authority/ghost_vm_bridge_capture.h>
[ghost-cpp]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/opening_authority/ghost_vm_bridge_capture.cpp>
[capture-53-68]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/opening_authority/type53_68_capture.h>
[bodies]: <C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp>
[auth-h]: <C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/sensor_auth_update.h>
[encoder]: <C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/activity_sensor_auth_encoder.cpp>
[route]: <C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/activity_message/activity_message_route.cpp>
[snapshot]: <C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp>
[push]: <C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/push/activity/activity_roster_push.cpp>
[world]: <C:/Destiny 2 Development/Dawn/src/state/activity/activity_world_arrival.cpp>
[arrival-route]: <C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp>
[probe]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp>
[teleport]: <C:/Destiny 2 Development/Dawn/src/client/hooks/teleport/runtime.h>
[catalog]: <C:/Destiny 2 Development/Dawn/src/state/build_data/runtime/build_data_catalog_runtime.cpp>
[tuner]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/forest_tuner_state.h>
[forest-ui]: <C:/Destiny 2 Development/Dawn/src/client/ui/forest/forest_panel.cpp>
[tower-beats]: <C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/tower_watch_cue_manifest.h>
[bootstrap]: <C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/towerfall_executor_bootstrap.cpp>
[roster-groups]: <C:/Destiny 2 Development/Dawn/src/client/content/scenarios/scenario_roster_groups.cpp>
[omega-inventory]: <C:/Destiny 2 Development/Dawn/exports/omega_inventory.md>
[manifest]: <C:/Destiny 2 Development/Dawn/exports/towerfall_cue_manifest.json>
[mapping]: <C:/Destiny 2 Development/Dawn/exports/towerfall_cue_mapping.json>
[sources-json]: <C:/Destiny 2 Development/Dawn/exports/towerfall_cue_sources.json>
[edges]: <C:/Destiny 2 Development/Dawn/exports/towerfall_cue_edges.md>
[tower-safe]: <C:/Destiny 2 Development/HANDOFF-HOMECOMING-TOWERFALL-SAFE-TRACE-2026-08-27.md>
[omega-script]: <C:/Destiny 2 Development/OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md>
[omega-complete]: <C:/Destiny 2 Development/OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md>
[guide]: <C:/Destiny 2 Development/internets guide to bungie bullshit.md>
