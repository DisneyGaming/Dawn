# Omega authoritative progression — next-steps handoff

Date: 2026-08-23  
Workspace: `C:\Destiny 2 Development`  
Target: Destiny 2 PC / Sunrise / `mission_scot` (Omega)

## Executive summary

Omega's client-side authored content is present and working through the sensor-report boundary.
The exact six-group roster restores the Ghost objective marker, Destiny constructs the native
mission-related objects, and the client detects the opening condition. The current deployed build
then proves the complete inbound sequence:

```text
235-byte bootstrap
  -> exact six-group roster acknowledgement
  -> host observer ready

57-bit type-1 follow-up
  -> parsed and logged
  -> readiness preserved

D00142CF / type 30 / index 20 entered
  -> exact opening edge recognized
  -> ready_to_triggered latched once
  -> host_action=latch_only
  -> outbound=none
```

The missing boundary is now precise: Sunrise does not turn that validated client condition into
authoritative mission state. Destiny's native script manager, scene, gate, director, and spawner
systems remain dormant because no evidence-backed host transition is committed or published.

The next task is **not** to manually spawn a scene, force script-manager memory, or send a guessed
type-5 packet. The next task is to recover the smallest authentic authoritative state change that
follows the opening edge, first as a no-output candidate plan and only later through the normal
authority publication path.

## Current deployed build

- Source: `C:\Destiny 2 Development\Sunrise-src`
- Build DLL: `C:\Destiny 2 Development\Sunrise-src\build\x64\Release\steam_api64.dll`
- Live DLL: `C:\Destiny 2 Development\bin\x64\steam_api64.dll`
- Live/build size: `12,103,168` bytes
- Live/build SHA-256:
  `68F944AA7BA903A9527B7E06860BEE02C83A4B496556E061D3125C454D06FB16`
- Previous live DLL backup:
  `C:\Destiny 2 Development\.sunrise\backup\steam_api64.20260823-103913.dll`
- Backup SHA-256:
  `CA166AFEB7A8A2253AED91BBB13D55CD0D25AD175CDEA09EA9EEC5CA7F66BA02`
- Active log: `C:\Destiny 2 Development\bin\x64\Sunrise\logs\sunrise.log`
- Parser fixture harness:
  `C:\Destiny 2 Development\tmp\sense_update_fixture_test.cpp`

Runtime settings remain:

```json
"roster_force_authored": true,
"seed_authored_sensors": false
```

Do not rerun the official Sunrise installer during this investigation. Its install state still
points at release 0.2.1, so the installer will replace the custom DLL.

## Latest proven runtime sequence

### 1. The 235-byte bootstrap decodes completely

At `t=120406`:

```text
stage=sensor_sense_update
result=decoded
parse=recovered
validation=ok
packet=1
bytes=235
consumed_bits=1875
padding_bits=5
roster_entries=6
groups=2
objects=6
```

The two changed-sense groups are:

```text
BA5F26EF  group_bits=334  objects=2
D00142CF  group_bits=606  objects=4
```

The six objects are:

```text
BA5F26EF  type 23 / index 1   body 167
BA5F26EF  type 70 / index 2   body 55
D00142CF  type 1  / index 0   body 85
D00142CF  type 43 / index 1   body 75
D00142CF  type 23 / index 16  body 167
D00142CF  type 70 / index 17  body 55
```

The observer then reports:

```text
stage=omega_opening_observer
result=ready
validation=ok
bootstrap_type1_bits=85
ready=1
triggered=0
host_action=none
outbound=none
```

### 2. The standalone type-1 update is valid but does not arm progression

At `t=120469`, the client sends a 39-byte update for `D00142CF`, type 1, index 0:

```text
body_bits=57
body=0x010FF24800000004
parse=recovered
validation=ok
observer_reset=none
```

This proves type 1 is schema-variable. The parser currently accepts only the recovered widths
`57`, `85`, and `92`, and requires exactly one width to land on a valid group boundary. The 57-bit
standalone form has no roster acknowledgement and cannot independently set readiness.

### 3. The exact opening edge latches once

At `t=140719`:

```text
stage=sensor_sense_update
result=decoded
parse=recovered
validation=ok
observer=latched
host_action=latch_only
packet=3
bytes=45
```

The decoded object is:

```text
registry=D00142CF
slot=30/20
group_bits=156
body_bits=100
body=F000000030000000 / 0000000000000002 / 0000000000000000
```

The transition line is:

```text
stage=omega_opening_observer
result=latched
validation=ok
ready=1
triggered=1
proposed=ready_to_triggered
host_action=latch_only
outbound=none
```

There were no parser failures, validation failures, observer resets, or `not_ready` results.

### 4. Nothing authoritative changes after the latch

This is expected for the current experiment:

```text
activity authority changed count=0
trigger authority resolved=0
spawner_index=FFFFFFFF
script_index=FFFFFFFF
director_index=FFFFFFFF
scene_index=FFFFFFFF
gate_index=FFFFFFFF
spawner requested=0
spawner active=0
mission director active=0
```

The experiment deliberately creates no `ActivityPlan`, no state transaction, and no outbound
message. It proves event identity and ordering only.

## What is Destiny-native versus Sunrise code

The activity-script manager shown in the logs belongs to the Destiny client. Sunrise did not
implement a mission-script interpreter.

Destiny-native systems observed through hooks include:

- activity-script manager and definitions
- authored condition/sensor components
- mission director runtime
- scene, gate, dialogue, monitor, and spawner runtime structures
- authoritative changed-object collect/apply/finalize machinery

Sunrise currently provides:

- roster discovery/publication and the exact Omega compatibility roster
- ordinary activity authority messages used during registration
- hooks/probes around Destiny's native systems
- bounded decoding of recovered type-6 forms
- a connection-local `ready`/`triggered` observer

The observer is not a mission engine. Its two booleans do not feed an encoder, scene system,
spawner, script manager, or durable activity state.

## Architecture supported by the evidence

```text
Sunrise roster publication
  -> Destiny registers authored client objects
  -> Destiny constructs native mission/runtime components
  -> Destiny detects authored local conditions
  -> Destiny sends activity message type 6 to the host
  -> MISSING: host evaluates mission policy and commits authoritative state
  -> expected normal authority publication back to Destiny
  -> Destiny applies state and activates authored script/scene/dialogue/spawner behavior
```

PS4 v1.59 and PC correlation establish that type 6 is client-to-host. Within the recovered sensor
wire pair, type 5 is the demonstrated host-to-client authority-apply path. That does **not** justify
manually constructing a type-5 "spawn scene" command. If type 5 is eventually used, it must be the
serialization of a proven committed mission-state change through the existing publication path.

## Recommended next steps

### Phase 1 — preserve the working baseline

Every experiment must retain all of these conditions:

```text
mission_scot selected
groups=6 top=3 sub=1 subkeys=3 objects=57
Ghost objective marker visible
235- or 236-byte bootstrap parses completely
omega_opening_observer result=ready
D00142CF type 30/index 20 latches exactly once
no duplicate actors or object-lifetime errors
```

Do not change roster discovery, group order, cache format, sensor seeding, player spawn gating, or
the current opening matcher while investigating the output side.

### Phase 2 — recover the first authoritative state target

The highest-value question is:

> After the real host accepts `D00142CF/type-30/index-20 entered`, which authoritative object or
> objects change, and to what state?

Investigate three evidence sources in parallel conceptually, but do not merge speculative results:

1. **PC client authority consumer**
   - Start at the native sensor-authority apply path, not at a generic lifecycle initializer.
   - Trace writes that change `definition_activity`, `definition_enabled`, and
     `definition_pending`.
   - Trace the first transition that resolves script, director, scene, gate, or spawner indices.
   - Record the owning registry key, slot type/index, body width, and decoded fields.

2. **Authored package data**
   - Search installed Omega content for condition/action relationships involving
     `D00142CF/type-30/index-20`.
   - Determine whether its consumer is the `D00142CF/type-43/index-1` scene,
     `D00142CF/type-23/index-16` mission object, `BA5F26EF/type-23/index-1`, or another authored
     component.
   - Do not infer an edge solely from proximity, ordering in the roster, or numeric adjacency.

3. **Archived Sunrise experiment**
   - Treat archived scene/ready/trigger/portal bodies as candidate constants only.
   - Identify which candidate body was intended immediately after the opening latch.
   - Compare each candidate with the PC decoder and current object schema before considering it
     valid.
   - Do not restore the archived state machine wholesale; it previously produced duplicate and
     T-pose actors.

The first decision gate is evidence convergence: at least two sources should identify the same
target object and compatible state semantics.

### Phase 3 — add a no-output candidate transition plan

Extend the current latch diagnostics to compute, but not send, a candidate authoritative diff.
The log should include:

```text
input session and patch epoch
current durable mission phase
input key/type/index/body hash
candidate output key/type/index
candidate prior and next body widths/hashes
candidate revision/state sequence
candidate publication group
evidence source for the candidate
host_action=candidate_only
outbound=none
```

Keep the candidate plan separate from `ActivitySensorObservation`. The current observer is
connection-local diagnostics; a real transition ultimately belongs to activity-session state and
must survive reconnects without leaking across sessions or epochs.

Do not turn the candidate into an `ActivityPlan` yet.

### Phase 4 — validate the candidate offline

Before runtime publication:

1. Encode the candidate into a scratch buffer only.
2. Decode it again through the matching schema/parser.
3. Verify exact registry, slot, body, revision, epoch, and group boundaries.
4. Confirm that unrelated roster slots are unchanged.
5. Confirm the candidate cannot remove `82FB58B7` or break the objective marker.
6. Confirm duplicate type-6 input produces no second candidate transition.
7. Add fixtures for wrong handle, wrong epoch, wrong destination, leave-body input, slot `30/24`,
   duplicate opening input, and bootstrap-reset ordering.

The offline result should be a structured state diff, not merely a byte string that the client
happens to accept.

### Phase 5 — implement the smallest evidence-backed host transition

Only after the candidate is validated:

1. Add a durable per-activity Omega phase with explicit allowed transitions.
2. Bind it to activity session, destination, patch epoch, and roster generation.
3. Commit the first transition exactly once.
4. Mark only the proven authoritative object dirty.
5. Let the existing normal authority publication system serialize the changed state.
6. Do not call a scene/spawner function directly and do not manually inject a one-off type-5
   packet from the type-6 route.

The desired structure is:

```text
type-6 route
  -> validate event
  -> propose activity-session transition
  -> commit state transaction
  -> normal dirty-object collector/publication
  -> client applies normal authority update
```

If no normal host publication path can represent the proven state, stop and document that missing
infrastructure instead of bypassing it with raw packet injection.

### Phase 6 — observe the native client reaction

For the first real transition, compare immediately before and after:

```text
activity-script definition activity/enabled/pending
selected/registered/active script-manager state
slot-35 and slot-18 runtime words/hashes
changed-object collector count and resolved indices
scene/gate/director/spawner indices
spawner requested/count/generation/active/mode
dialogue and entity creation
new type-6 reports
objective-marker visibility
roster generation and object count
```

Success is a coherent native reaction, not merely an accepted packet.

### Phase 7 — map the next authored checkpoint

Previous runs repeatedly observed `D00142CF/type-30/index-24` after the opening point:

```text
entered body second=2
left body second=4
another entered/state variant body second=6
```

Treat index 24 as the leading candidate for the next client condition only after the first
authoritative opening transition produces a real script/scene reaction. Do not advance on index 24
while the opening phase remains dormant.

Repeat the same evidence-first loop for every later phase:

```text
client condition
  -> validated type-6 input
  -> proven host state transition
  -> normal authority publication
  -> native client reaction
  -> next condition
```

## Immediate task for the next engineer/agent

Start with a read-only trace of the PC authority consumer and answer this narrow question:

> Which native authoritative component update changes the Omega activity-script definition from
> `activity=-1/enabled=0` or resolves the first scene/director/spawner object?

Produce a candidate report containing:

```text
target registry key
slot type/index
body schema and field meanings
native consumer function/RVA
fields changed in the script/scene/director/spawner runtime
evidence connecting it to the opening edge
confidence level and unresolved assumptions
```

Do not implement an outbound transition until that report exists.

## Relevant source files

- `Sunrise-src/Sunrise/src/middleware/bap/activity_message/sense_update.h`
- `Sunrise-src/Sunrise/src/middleware/bap/activity_message/activity_sense_update_parser.cpp`
- `Sunrise-src/Sunrise/src/server/bap/encrypted/activity_message/activity_message_route.cpp`
- `Sunrise-src/Sunrise/src/server/bap/internal.h`
- `Sunrise-src/Sunrise/src/server/bap/encrypted/bap_connection_publication.cpp`
- `Sunrise-src/Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`
- `Sunrise-src/Sunrise/src/middleware/bap/activity_message/activity_sensor_auth_encoder.cpp`
- `Sunrise-src/Sunrise/src/client/hooks/bootflow/activity_script_upstream_probe.cpp`
- `Sunrise-src/Sunrise/src/client/hooks/bootflow/activity_schema_decode_probe.cpp`
- `Sunrise-src/Sunrise/src/state/activity/activity_world_arrival.cpp`

Reference analysis:

- `_analysis/ps4-eboot-159-sense-host-boundary-20260823.md`
- `_analysis/omega_pc_activity_init_findings_20260823.md`
- `OMEGA_ACTIVITY_SCRIPT_HANDOFF_2026-08-23.md`
- `HANDOFF-OMEGA-ACTIVITY-INIT-20260823.md`
- `OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md`

Archived candidate implementation root:

```text
C:\Destiny 2 Development\deliverables\Sunrise-complete-source-and-Release-20260822\source\Sunrise-src\Sunrise\src
```

## Build and deployment commands

Build without deploying:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1" -BuildOnly
```

Deploy after Destiny 2 is closed:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1"
```

Do not clear the content cache for server-only routing, state, or logging changes. Clear it only if
roster/content discovery changes, and expect a slow rebuild.

## Stop conditions

Stop and roll back the current experiment if any of these occur:

- roster is no longer exactly six groups / 57 objects
- Ghost objective marker disappears
- bootstrap no longer produces `result=ready`
- opening produces `not_ready`, `rejected`, or more than one latch
- duplicate/T-pose actors appear
- object release/lifetime errors appear
- player spawn regresses to a load stall
- scene, gate, or spawner activates before its owner/runtime is initialized
- a transition affects another destination or another session/epoch

The current deployed DLL backup is known and recoverable through the project deployment script.

## Known limitations that must remain explicit

1. Type-1 and type-43 variable widths are selected from recovered bounded allowlists using envelope
   boundaries. Their internal optional-field grammars are not fully decoded.
2. The current parser is a recovered Omega subset, not a universal type-6 parser.
3. Readiness recognizes the two complete measured bootstrap variants. The standalone 57-bit update
   does not grant readiness.
4. The observer is connection-local and diagnostic-only. It is not suitable as durable mission
   state.
5. No authoritative output body after the opening latch has been proven.
6. The port-30976 channel timeout seen at the end of recent runs is a separate connectivity issue.
   The latch itself completed earlier with `outbound=none`; do not conflate the two without new
   evidence.

## Current accurate status statement

> Omega authored navigation, runtime construction, sensor decoding, bootstrap readiness, and the
> exact opening trigger are proven. Destiny's native mission machinery is present but dormant.
> Sunrise still lacks the evidence-backed authoritative state transition that follows the opening
> condition. No manual scene spawn, guessed mission sequence, or ad hoc type-5 output has been
> implemented.
