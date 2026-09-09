# Omega activity/script handoff — 2026-08-23

## Executive summary

Omega (`mission_scot`) now loads with the exact six-group roster from the run where Ghost
navigation worked. Objective markers are confirmed working again.

The latest log also provides the clearest evidence so far for how mission progression is split:

1. The roster registers the authored client objects.
2. The client constructs the mission director/runtime components.
3. The client detects authored sensor conditions and sends activity message type 6 to the host.
4. The host is expected to turn those sensor reports into authoritative mission state.
5. The client consumes that authoritative state and activates scripts, scenes, spawners, dialogue,
   gates, and later mission phases.

Steps 1–3 now happen. Step 4 does not. The current Sunrise server deliberately parses and logs the
type-6 sense update with `host_action=none`, then returns success without changing state. This is
the current concrete boundary—not a generic “init is missing” problem.

## Current working build

- Workspace: `C:\Destiny 2 Development`
- Source: `C:\Destiny 2 Development\Sunrise-src`
- Live DLL: `C:\Destiny 2 Development\bin\x64\steam_api64.dll`
- Live DLL size: `12,092,928` bytes
- Live DLL SHA-256:
  `CA166AFEB7A8A2253AED91BBB13D55CD0D25AD175CDEA09EA9EEC5CA7F66BA02`
- Active log: `C:\Destiny 2 Development\bin\x64\Sunrise\logs\sunrise.log`
- Marker-working comparison log:
  `C:\Destiny 2 Development\tmp\omega_handoff_log\evidence\sunrise.latest.log`
- Current generated scenario cache:
  `C:\Destiny 2 Development\bin\x64\Sunrise\cache\build_data.bin`
- Current content manifest:
  `C:\Destiny 2 Development\bin\x64\Sunrise\cache\content_manifest.bin`

Runtime `settings.json` has:

```json
"roster_force_authored": true,
"seed_authored_sensors": false
```

The source default for `roster_force_authored` remains false. The live setting explicitly enables
the Omega compatibility path.

## What the build does right now

### Omega roster override

When `roster_force_authored` is enabled, content discovery recognizes four exact Omega authored
keys:

```text
82FB58B7
D00142CF
BA5F26EF
F7A6CE7F
```

If all four are discovered, the dynamic Omega mission subset is replaced by this measured six-key
roster:

```text
4786C0E0  player/runtime group
29D7B029  type-19 group
82FB58B7  directive/objective, dialogue, and music sensors
BA5F26EF  authored mission group
D00142CF  Ikora/gate/scene/opening-volume group
F7A6CE7F  authored dialogue/mission group
```

If discovery does not find all four Omega-specific keys, publication removes any incomplete forced
subset and falls back to the dynamic roster. Other destinations remain dynamic.

Implementation locations:

- `Sunrise/src/client/content/scenarios/scenario_roster_groups.cpp`
  - exact four-key set near line 18
  - measured-key mask near line 178
- `Sunrise/src/client/content/scenarios/scenario_roster_build.cpp`
  - records the measured Omega key mask near line 77
- `Sunrise/src/client/content/scenarios/scenario_roster_publish.cpp`
  - exact six-key output near line 13
  - replacement/fallback logic near line 27
- `Sunrise/src/core/settings/client/definition.h`
  - setting near line 49
- `Sunrise/src/core/settings/client/client_settings_parser.cpp`
  - JSON parser near line 59

### Current host handling of client sensor reports

The current server does **not** advance Omega from sensor reports.

`Sunrise/src/server/bap/encrypted/activity_message/activity_message_route.cpp`:

- `report_sense_update` begins near line 83.
- Its log explicitly says `host_action=none` near line 94.
- The type-6 route near lines 491–495 calls the logger and immediately returns true.

That means the host acknowledges the one-way packet but stages no state transaction and publishes
no mission transition in response.

### Current logging/probes

This build retains activity and spawn diagnostics, including:

- roster result and exact group layout
- join/membership phases
- slot-35 mission director initialization/apply
- slot-18 and slot-35 runtime snapshots
- activity-script manager state
- authored sensor reports
- trigger-authority/spawner snapshots
- player spawn hold/release and native spawn gate
- world transition state

The probes are observation-only unless a log explicitly says otherwise.

## Proven observations from the latest run

### 1. The exact known-good roster is restored

At `t=71969`:

```text
stage=roster result=ok groups=6 top=3 sub=1 subkeys=3 objects=57
```

The exact layouts are:

```text
4786C0E0: 16,35,18,17,41,13...
29D7B029: 19
82FB58B7: 68,11,53
BA5F26EF: 4,23,70
D00142CF: 1,43,4,4,4,26,26,4,4,4,4,4,4,4,4,4,23,70,31,31,30,34,34,34,30
F7A6CE7F: 30,30,57,57
```

The user confirmed that opening Ghost shows the objective marker. This directly validates that the
missing navigation regression was caused by the wrong roster topology, especially omission of
`82FB58B7`.

### 2. Player/world initialization succeeds

The spawn probe reports:

```text
stage=spawn_gate result=native_allowed
world_ok=1
world_state=3
local_ready=1
script_ok=1
director_ok=1
```

So the remaining mission failure is not the earlier infinite-load/no-roster failure.

### 3. Native mission runtime objects are constructed

At approximately `t=84469–84484`:

```text
stage=mission_director_runtime_initialize
stage=mission_director_runtime_apply
stage=mission_runtime_change slot=35 ... q0..q7=0
stage=mission_runtime_change slot=18 ... q0=1
```

Interpretation:

- Slot 35 exists, but its authoritative runtime body is neutral/all zero.
- Slot 18 exists and has its initial `q0=1` state.
- Construction alone does not make Omega progress.

### 4. The client sends authored sensor data to the host

At `t=84484`, a 236-byte type-6 packet was captured. The current parser reports no single leading
object, but its captured body visibly contains `BA5F26EF` and `D00142CF`. This likely represents a
batched/multi-object form that the current summary parser does not fully enumerate.

More importantly, at `t=109500` the client sends an exactly parsed opening-volume edge:

```text
stage=sensor_sense_update
parse=captured
host_action=none
object=1
key=0xD00142CF
slot=30/20
body_bits=100
bodyFirst=0xF000000030000000
bodySecond low bits=2
```

This is the exact Omega opening trigger previously identified in archived code.

### 5. The host ignores the exact opening event

No `omega_opening_transition` occurs in the current run. The server logs the exact trigger as
`observed_unhandled` with `host_action=none`.

This is the strongest current evidence that trigger sensing is client-side while progression is
host-authoritative.

### 6. Spawner/script authority does not resolve or activate

Repeated snapshots remain:

```text
count=0
resolved=0
spawner_index=4294967295
script_index=4294967295
director_index=4294967295
scene_index=4294967295
requested=0
counts=0,0,0,0,0,0
generation=0
active=0
mode=0
```

The activity-script manager also remains effectively inactive:

```text
selected=0
registered=0
active=0
definition_activity=-1
definition_enabled=0
definition_pending=0
```

Therefore the roster creates enough client state for navigation and sensor detection, but it does
not itself install/enable the authoritative activity definition or request the spawner.

## What is proven versus still theoretical

### Proven

1. The dynamic roster produced the wrong Omega topology.
2. The exact six-group roster restores Ghost objective navigation.
3. The client constructs slot 35 and slot 18 with the exact roster.
4. Slot 35 remains neutral/all zero.
5. The client detects and reports the exact `D00142CF/type-30/index-20` opening event.
6. The current Sunrise host intentionally performs no action for that event.
7. Spawner/script state remains unresolved and inactive.
8. Calling a native lifecycle initializer by itself is not enough. The earlier experiment reached
   later internal stages but did not restore markers or populate the authoritative definition.

### Strongly supported theories

#### Theory A — client sensing, host progression

The client owns high-frequency local sensing and sends meaningful edges to the activity host. The
host owns durable mission phase and sends authoritative component changes back. This is the normal
latency-friendly split: animations and local sensing need not round-trip every frame, while mission
state remains synchronized.

The exact inbound opening edge plus `host_action=none` strongly supports this model.

#### Theory B — the next missing link is an event-to-state transition

The next missing operation is likely not another generic init. It is a mapping resembling:

```text
type-6 sense update
  -> D00142CF / type 30 / index 20 entered
  -> latch Omega opening phase
  -> publish the correct authored authoritative body
  -> native client scene/script/spawner reacts
```

The exact outbound body and timing after the latch are still unproven.

#### Theory C — the activity definition is present structurally but not enabled

The activity-script manager obtains a definition pointer later in startup, but continues to report
`definition_activity=-1`, `definition_enabled=0`, and `definition_pending=0`. It may be waiting for
an authoritative mission-phase/component body rather than a direct function call.

#### Theory D — the 236-byte batched type-6 packet contains additional useful edges

The packet at `t=84484` includes multiple authored keys in its capture, while the current parser
only summarizes a single optional object. Fully decoding the batched form may reveal scene-ready,
gate, dialogue, or initial script conditions that occur before the explicit opening volume.

#### Theory E — the authored sensor seed/order matters

The current setting is `seed_authored_sensors=false`. Archived experiments attempted a staged
sequence of neutral scene seed, ready/gateway state, trigger state, portal state, and completion.
That suggests ordering may be important: registering an active spawner too early can duplicate
actors or create invalid scene state. This sequence is historical evidence, not yet validated by
the current clean baseline.

### Weaker/uncertain theories

1. Bubble/slice selection alone triggers the mission. The working roster proves topology matters,
   but it does not prove that loading slice-set 0 or region/slice 120 should automatically start the
   authored script.
2. Slot 35 alone starts everything. Slot 35 is constructed and initialized but remains neutral.
3. The mission is fully client-driven. The client detects the event, but no progression happens
   when the host ignores it.
4. A native lifecycle `stage 3` retry is the missing answer. The earlier forced lifecycle test did
   not restore the roster/navigation and did not populate spawner authority.

## Archived host-transition implementation

A previous source snapshot contains an experimental Omega transition path:

```text
C:\Destiny 2 Development\deliverables\Sunrise-complete-source-and-Release-20260822\source\Sunrise-src\Sunrise\src
```

Relevant archived files:

- `server/bap/encrypted/activity_message/activity_message_route.cpp`
  - matches `D00142CF/type-30/index-20`, 100 bits, entered body
  - calls `mark_omega_opening_triggered()`
- `state/activity/forced/activity_forced_destination.cpp`
  - latches opening, scene-handoff, and completion flags
- `server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`
  - advances an `activityOmegaOpeningStage` state machine
- `middleware/bap/activity_message/activity_sensor_auth_encoder.cpp`
  - emits selected authored group bodies for scene/ready/trigger/portal/completion stages
- `server/bap/encrypted/push/activity/activity_keepalive_push.cpp`
  - accelerates roster publication while the staged opening is in progress

Do **not** restore this tree wholesale. It is a complex prior experiment containing assumptions
about exact auth bodies, actor lifetime, scene ordering, and portal handoff. Comments in that code
explicitly record earlier duplicate/T-pose actor failures. Use it as a map of the suspected state
machine and as a source of candidate constants, then port/test one transition at a time against the
current clean baseline.

## How to determine exactly where/how scripts are triggered

### Phase 1 — preserve this baseline

Before each experiment, require all of the following:

```text
groups=6 top=3 sub=1 subkeys=3 objects=57
objective marker works
spawn_gate result=native_allowed
slot 35 and slot 18 constructed
D00142CF/type-30/index-20 event captured
```

If any condition changes, the test is no longer isolating script progression.

### Phase 2 — finish decoding inbound type-6 reports

Extend `sense_update` parsing to enumerate every object/slot entry in the 236-byte batched packet.
Log each entry as:

```text
packet sequence
entry ordinal
registry key
slot type/index
body bit length
body words/hash
```

Do not mutate state during this phase. The goal is an exact ordered list of client-detected events
from world entry through opening-volume entry.

### Phase 3 — trace the host transition boundary

Add an observer around the point where a supported sense update would stage a transaction. Record:

```text
input event identity
current mission phase
proposed next phase
outbound roster/auth groups selected
exact changed slots
packet revision/state sequence
```

First test a latch-only implementation that records the proposed transition but does not encode an
outbound body. This proves event matching and deduplication without risking the client object graph.

### Phase 4 — determine the minimal authoritative output

Use single-variable tests. Candidate outputs, in increasing risk order:

1. Change only the mission runtime/script scalar associated with the opening phase.
2. Publish only the neutral/ready authored scene record.
3. Publish only the exact post-trigger mission-runtime record.
4. Publish a scene/spawner record only after a native readiness observation confirms its owner and
   allocator state.

For each test, compare before/after:

```text
slot-35 runtime hash and words
activity definition enabled/pending/activity fields
spawner requested/count/generation/active
scene/gate candidate indices
new client type-6 events
dialogue/animation/entity creation
```

Stop immediately on duplicate actors, T-poses, object-release errors, loss of the objective marker,
or roster generation changes.

### Phase 5 — correlate with the regular Destiny 2 binary in Ghidra

The most useful runtime anchors in the current PC binary are already logged:

```text
activity_script_manager_update_loop  RVA 0x1764EC0
activity_script_lane1_active_count   RVA 0x175C290
activity_script_manager_authored_start (previously identified) RVA 0x1773200
```

Recommended reverse-engineering path:

1. Start from the function that consumes the authoritative component update, not from the generic
   initializer.
2. Follow writes to the fields observed as `definition_activity`, `definition_enabled`, and
   `definition_pending`.
3. Identify what message/component class supplies those writes.
4. Follow the first transition from pending/disabled to selected/registered/active.
5. Correlate that call with the exact outbound auth group/slot emitted by Sunrise.
6. Separately follow the type-30 sensor component's producer to confirm that the inbound type-6
   packet is an event notification rather than the script executor itself.

The PS4 v1.59 EBOOT can help label architecture and equivalent lifecycle functions, but the regular
PC binary is required to match the live RVAs, layouts, and probe observations used here.

## Known failed paths and pitfalls

### Additive forced groups

Appending the four measured groups to the newer dynamic Omega groups overflowed the bubble-group
publication. The generated scenario row returned `result=no_groups`, `spawn_state` stayed zero,
and the client appeared to load forever before tearing down the activity session.

The current build fixes this by replacing the dynamic Omega mission subset with the exact six-key
topology.

### Native lifecycle forcing

The forced lifecycle runner advanced internal flags/stages but retried stage 3 indefinitely. It did
not restore the objective marker and did not resolve the spawner. That experiment has been removed
from the active caller path.

### Identity/roster gating

Holding the roster until later identity/type-23 reflection removed the marker and did not solve
mission progression. Those behavioral changes were rolled back.

### Old DLL backup incompatibility

An older 13 MB DLL was incompatible with the current content and caused Steam's “Problem reading
game content” error. Do not use arbitrary `.sunrise\backup` DLLs as a rollback strategy. Rebuild
from the current source and verify the live/build SHA-256 match.

### Cache behavior

Roster-builder changes do not affect an already generated `build_data.bin`. After changing content
discovery/publication, close Destiny 2 and use:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1" -ClearCache
```

The next launch will be slower while the content cache regenerates. Do not clear the cache for
server-only routing or logging changes.

## Suggested next experiment

The safest high-value next experiment is:

1. Keep the exact six-group roster unchanged.
2. Fully enumerate the 236-byte batched type-6 update.
3. Add a no-output/latch-only matcher for the exact opening edge.
4. Confirm exactly one transition from Omega ready to triggered.
5. Log which archived authoritative body would be selected, but do not send it yet.

That will determine whether the script chain is event-driven and provide a precise outbound target
without risking the currently working navigation baseline.

## Success criteria for the actual mission fix

A real fix requires more than a marker:

```text
exact six-group roster remains stable
objective marker remains visible
opening trigger is latched once
activity definition becomes enabled/pending or active
spawner/scene indices resolve from FFFFFFFF to valid values
requested/count/generation or scene state changes coherently
dialogue/animation/entities appear without duplicates
mission advances without proximity hacks beyond authored triggers
```

Until those occur, the current result should be described as “authored navigation and sensing
restored; authoritative mission progression still missing.”
