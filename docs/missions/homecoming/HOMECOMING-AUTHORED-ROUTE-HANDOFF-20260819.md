# Homecoming authored-route and manager handoff

Status: 2026-08-19, after the Homecoming run recorded in `bin/x64/Sunrise/logs/sunrise.log`.

This is the current evidence report and continuation handoff for the Destiny 2 Sunrise offline
Homecoming project. It reconstructs the investigation from the imported working snapshot through the
first fully observed authored client route and manager lifecycle. It distinguishes the client handoff
that is now proven from the authored mission execution that is still missing.

## Executive verdict

The newest run confirms, on one manager pointer (`000001B43F454EA8`), all four requested milestones:

1. The client produced an authored lane-2 descriptor with `route=1`, then committed `route=1` twice.
2. The native authored manager starter created a new manager through caller RVA `+0x17905B7` and
   assigned it identity 2.
3. That manager entered mode 1 through the native mode setter with payload size 6136 bytes, exactly
   `0x17F8`, from caller RVA `+0x17B3971`.
4. The same pointer progressed normally through modes `1 -> 2 -> 4 -> 5`, with the expected native
   payload sizes `0x17F8`, `0x1880`, `0x90`, and `0xA0`.

Confidence in those four statements is very high. They are direct runtime observations rather than an
inference from UI state or a collaborator's address map.

The run also establishes the next boundary:

- Sunrise queued a 136-byte kind-22 host-reestablish message.
- The client decoded kind 22 and entered the verified manager activation function at `+0x177E940`.
- The function returned success (`result=1`), but the manager's active identity field at `+0x1AF00`
  remained `0` before and after the call.
- The existing `identity_enable` recorder logged no invocation.
- The manager still later advanced from mode 4 to mode 5.

Therefore the precise current claim is:

> Authored route selection, route commit, authored manager creation, and the native manager lifecycle
> through established mode 5 are proven. Full authored mission activation and execution are not yet
> proven. The first directly measured unresolved point is a successful return from `mgr_activate`
> without the sole `+0x1AF00 = 1` write occurring.

## Evidence standard and sources

The report uses four evidence classes:

- The newest single-run log: `C:/Destiny 2 Development/bin/x64/Sunrise/logs/sunrise.log`.
- The current source tree and uncommitted implementation diff on branch `red-war-gameplay-host`.
- The pinned unpacked client image: `destiny2_unpacked.bin`, base `0x7FF618070000`, where file offset
  equals RVA.
- Local Ghidra decompilation and cross-reference scripts under `ghidra_scripts/`.

Collaborator information is included only when it was corroborated against our own dump or live log.
Earlier names that were later disproved are retained only in the historical section and marked as such.

Current deployed DLL:

- SHA-256: `2C0086E87BFF12FD89944D5A2A18AF338649221D17DDF3305718CC1D0E79580E`
- MD5: `EFB7AA5DF4C98A516CD3D2E421019C1F`
- Build output and live DLL hashes match.
- Git branch: `red-war-gameplay-host`; HEAD: `1ea4cb7b455b`; the worktree contains the current
  investigation changes.

## Latest run: exact authored handoff

### 1. Matchmaking configuration materialized a usable lane

At `t=124844`, the native service-43 configuration consumer materialized descriptor 282 with one
configuration entry:

```text
log line 1833
stage=activity_matchmaking_lane_policy_materializer
descriptor_id=282 descriptor_present=1 nested_present=1 nested_count=1
lane_valid=1 lane_count=1
```

The immediately following rebuild changed the client lane state from empty tags to 282 and made lane 0
valid with one entry:

```text
log line 1834
stage=activity_matchmaking_lane_rebuild changed=1
tag_before=65535,65535,65535 tag_after=282,282,282
generation_before=0,0,0 generation_after=1,0,0
valid_after=1,0,0 count_after=1,0,0
```

This is the direct confirmation that the fixed configuration body no longer leaves the native provider
insertion loop at zero iterations.

### 2. The authored route was selected

At `t=132531`, lane 2 returned a real descriptor carrying `route=1`:

```text
log line 9508
stage=activity_script_route_descriptor lane=2 source=lane
descriptor=000001B43F4D1EA8 route=1
pointer18=0x7E63ED543453C5FA selector=4
opening=1 package=mission_towerfall
```

This is the route-selection observation. It is not a forced write: the recorder reports
`mutation=observe_only` on the later commit and update paths.

### 3. A new manager was created through the authored starter

At the same tick, a new manager was created through the authored caller:

```text
log line 9518
stage=activity_script_manager_start n=3 route=authored
caller_rva=0x17905B7 manager=000001B43F454EA8
identity_before=-1 identity_after=2
mode_before=0 mode_after=1
opening=1 package=mission_towerfall
```

The pointer had no identity before the call and identity 2 afterward. This is direct evidence of native
authored-manager construction, not reuse of the two earlier local mode-6 managers.

### 4. Native mode 1 used the exact 0x17F8 payload

The mode setter for that same pointer recorded:

```text
log line 9517
stage=activity_script_manager_mode_set
manager=000001B43F454EA8 identity_before=2 identity_after=2
mode_before=0 requested_mode=1 mode_after=1
payload_size=6136 caller_rva=0x17B3971
```

`6136 decimal = 0x17F8`. The native caller, requested mode, resulting mode, identity, manager pointer,
and payload size all agree.

### 5. The same manager progressed 1 -> 2 -> 4 -> 5

The complete sequence is:

| Time | Log line | Before -> after | Payload | Caller |
|---|---:|---|---:|---|
| 132531 | 9517 | 0 -> 1 | `0x17F8` | `+0x17B3971` |
| 132594 | 9578 | 1 -> 2 | `0x1880` | `+0x17B3E54` |
| 137203 | 11037 | 2 -> 4 | `0x90` | `+0x17B39E3` |
| 152781 | 16670 | 4 -> 5 | `0xA0` | `+0x17B3F97` |

Every row names manager `000001B43F454EA8` and identity 2. There is no pointer substitution between
steps.

### 6. Route 1 was committed and applied to that manager

At `t=137266`, the client committed route 1 twice and ran route update against the same manager while
it was in mode 4:

```text
log line 11074
stage=activity_script_route_commit descriptor=000001B43F4D1EA8 route=1
pointer18=0x7E63ED543453C5FA opening=1 package=mission_towerfall
mutation=observe_only

log line 11077
stage=activity_script_route_update manager=000001B43F454EA8
identity_before=2 identity_after=2 mode_before=4 mode_after=4
source_nonce=0x7E63ED543453C5FA mutation=observe_only

log line 11079
stage=activity_script_route_commit descriptor=0000002607BFFAD8 route=1
pointer18=0x7E63ED543453C5FA mutation=observe_only
```

The shared nonce ties route selection, commit, and manager update to the same authored launch.

### 7. Kind 22 now reaches mgr_activate, but active remains zero

Sunrise queued the recovered native 136-byte shape:

```text
log line 13173
server stage=host_reestablish result=queued
session=0x48084DE0FCD62765 machine=0x48084DE0FCD62765
address=0x7F000001 port=30976 decoded_size=136 identity=empty
```

The client decoded and dispatched it as event 22 with decoded size 136:

```text
log line 13232
stage=activity_script_wire_dispatch client_state=5 event=22 value=136
```

The direct recorder then observed the verified `+0x177E940` function:

```text
log line 13235
stage=activity_script_manager_activate caller_rva=0x16DF850
manager=000001B43F454EA8 result=1
identity_before=2 identity_after=2
mode_before=4 mode_after=4
active_before=0 active_after=0
```

Ghidra independently confirms:

- Function `+0x177E940` is a three-argument `bool` function.
- Its only static caller is the kind-22 dispatcher call at `+0x16DF84B`; the recorded return address is
  therefore `+0x16DF850`, exactly as expected.
- The only instruction in the pinned client that writes `manager+0x1AF00 = 1` is inside this function,
  at `+0x177F156`.

The direct conclusion is not "activation succeeded." The function accepted or handled the message in
some sense (`result=1`) but took a successful branch that did not execute the active-field write.

## What is proven, and what is not

### Proven now

- Steam API emulation, local SteamID, callbacks, and interface tables.
- Sign-in, character loading, and orbit.
- Local Demonware/BAP and gameplay networking.
- Destination selection and native 282 -> 266 Towerfall correction.
- Activity/session allocation and joining.
- Fireteam/roster flow.
- Mission package, map, opening banner, and world shell loading.
- A valid native matchmaking configuration entry is materialized.
- An authored lane descriptor with `route=1` is selected.
- `route=1` is committed and route-updated into the authored manager.
- A new identity-2 manager is created through the authored caller.
- That manager follows native modes `1 -> 2 -> 4 -> 5` with expected payload sizes.
- A 136-byte kind-22 message is sent, decoded, and routed into `mgr_activate`.
- The DLL builds and the deployed hash matches the build output.

### Not proven yet

- `identity_enable` executing in this run.
- `manager+0x1AF00` becoming 1. It directly remained 0 across `mgr_activate`.
- The manager selected field at `+0x87C` becoming nonzero.
- The identity definition enabled field at `definition+0x94C` becoming 1.
- Authored activity scripts, objectives, encounter machines, enemies, dialogue, or cinematics running.
- The 128-bit and 144-bit identity blocks in kind 22. Sunrise currently sends both blocks empty.
- Service-7's 128-byte activity metadata. The current response remains zero-filled, and current tracing
  did not identify it as the immediate authored-route gate.

## How we got here: reconstructed investigation history

### Phase 0 - Import a working offline baseline

The work began from the collaborator snapshot copied from Downloads into the active development tree.
That snapshot already provided the difficult foundation: Steam emulation, local identity, BAP and
Demonware emulation, gameplay networking, activity allocation/join, roster flow, and clean DLL builds.
The older `spawner` branch was preserved because it contained an entity spawner and earlier authored
experiments, but it was too divergent to merge wholesale. The current work stayed on
`red-war-gameplay-host` and treated the old branch as a reference, not as a source of truth.

### Phase 1 - Establish the real Homecoming destination path

Homecoming is `mission_towerfall`, activity 266, reached from the Farm through the native CHOSEN
transition 282 -> 266. The first durable achievement was preserving that native correction instead of
blindly replacing the activity index. The override now stages the desired destination and commits at the
CHOSEN prelaunch point, allowing service 6 and the client's own descriptor machinery to name the real
mission package.

The map and opening banner then loaded, but the mission remained empty. This separated content loading
from authored execution: packages and geometry were available, while objectives, encounter logic,
enemies, and cinematics were not.

### Phase 2 - Reverse the selection and manager machinery

Runtime probes and the unpacked dump mapped the important client chain:

```text
selection descriptor
  -> route descriptor lookup
  -> route commit / route update
  -> manager selection pump
  -> local starter (mode 6) or authored starter (mode 1)
  -> manager/component lifecycle
```

Several early explanations were disproved:

- The destination predicate was not rejecting 266. A direct runtime probe showed activity 266 was
  accepted once the destination datum existed. The actual issue was ordering: the local manager had
  already been created before the authored destination arrived.
- Replaying a complete 620-bit authored descriptor through activity message 1 did not populate the
  runtime holder used by route commit.
- A function initially named as selection ingest was experimentally shown to be a fireteam-join API;
  calling it produced a phantom fireteam and did not change the route.
- Directly forcing route bits, manager modes, lifecycle flags, or active fields caused resets, pruning,
  freezes, or misleading transient states. Those forces were retired in favor of observation and native
  inputs.

This period produced the durable instrumentation strategy used now: validate an RVA against the pinned
image, attach an observe-only detour, log caller/inputs/before/after, and correlate one pointer across a
single run.

### Phase 3 - Find and exercise the native authored trigger

The investigation found a real launch gate at `+0x1763B20` and dispatcher at `+0x134FDF0`. During the
Homecoming load, the global and lane phases naturally matched at phase 29; two readiness bytes blocked
the dispatch. A tightly scoped one-call experiment proved the dispatcher could run offline. Forcing its
authored discriminator made the native authored constructor execute for the first time in these tests.

That experiment also exposed the wrong model: with an empty constructor payload it attempted a null
fireteam join, and filling the payload with a local player identity attempted to join a specific player.
The path was real, but using it as a fabricated join target was not a valid offline mission launch. The
forcing hooks were parked.

### Phase 4 - Move the mission shell onto the public region path

`region_force_public` was the next durable keeper. It made the mission's region follow the public
citizen-join path used by patrol spaces instead of the dead private/fireteam path. This is what allowed
the world shell, slice set, and banner to load through a real online-style transition.

That exposed successive networking seams:

1. A phantom ambassador slot referred to a peer that does not exist offline.
2. Selecting the local peer as ambassador moved the client into matchmaking search.
3. Sunrise initially returned no region session.
4. The search-result protobuf nesting was recovered: service 43 field 3 contains repeated entries whose
   field 1 is a wrapper containing the 128-byte descriptor. It is not a numeric id.
5. NAT traversal to loopback and the QoS request/reply header were recovered.
6. Activity host session allocation and membership publication were aligned so the client could join the
   local session.

This work moved the project from "world cannot connect" to "world and session establish, but authored
content does not execute."

### Phase 5 - Fix Sunrise's service-43 configuration overwrite

The next root cause was subtler than a missing response. Sunrise sent the configuration field as present
but zero-length. The native decoder therefore committed a configuration with zero provider entries and
zero timing thresholds. That made the manager start advertising almost immediately, mutate the session
generation, clear the in-flight search, and prevent the inner provider worker from consuming the state-2
result.

The apparent "job only consumes state 1 once" theory was refined by direct timestamps: the outer world
controller did receive another pass and complete its configuration task. The actual failure was inside the
native matchmaking lane/provider state, not a missing generic scheduler repump.

The server fix now emits the smallest known-useful nested service-43 field-4 body:

- lane policy service configuration id 1;
- search-only threshold 60 seconds;
- desperation threshold 60 seconds;
- one present provider-policy entry, so the native insertion loop executes once;
- default bubble limits for player, posse, and matchmade capacity.

The latest run confirms the native materializer receives `nested_count=1`, produces `lane_valid=1`, and
publishes `lane_count=1`. This is the immediate predecessor to the observed `route=1` descriptor.

The search-result entry correction was kept separate: field 1 is the descriptor wrapper, while optional
fields 2 through 7 may be omitted. A one-level mis-nest makes the raw descriptor look like the wrapper
submessage and can silently discard the remaining entry fields even when the outer decode reports success.

### Phase 6 - Eliminate Service 7 as the immediate route gate

Service 7 still returns a discriminator/session header followed by 128 zero bytes. Instrumentation traced
the generic response handler, request callback, cache copy, and direct field references. The callback
copies the zero block into a private current-cache record, but the kind-10 activity-host dispatch does not
parse that block into the authored route, and no decisive direct reader was found on the route-creation
path.

This does not prove the metadata is unimportant forever. It does rule it out as the immediate prerequisite
for route selection, because route 1 and the authored manager now occur while the block is still zero.

### Phase 7 - Recover the real kind-22 wire body

The session-stream dispatcher was mapped by message kind. Kinds 30 and 38 update membership and
parameters and resolve the manager by session id. Kind 22 is host-reestablish and calls the manager
activation function after the same lookup.

The native registry and codec established a 136-byte decoded shape:

```text
sessionId u64
machineId u64
NetAddr (method-dependent serialization)
identity block: 128 bits
identity block: 144 bits
```

Sunrise now serializes that exact decoded shape and delays it until membership, the activity-host
parameter, and the player snapshot have been published. For the current experiment the two trailing
identity blocks are empty.

The newest run proves the complete transport chain: server queue -> client event 22/136 -> manager lookup
-> `mgr_activate`. The old statement "activation never runs because kind 22 never arrives" is therefore
obsolete. The refined current statement is "kind 22 reaches `mgr_activate`, but the call returns true
without taking its active-field write branch."

### Phase 8 - Achieve the native authored client handoff

With the configuration lane fixed and the session path stable, the client produced route 1 naturally,
created a new identity-2 manager through the authored caller, committed the same route and nonce, and
advanced the manager through the native setup ladder to mode 5. This is the current milestone.

It is a client handoff milestone, not yet a playable authored mission. The distinction matters because
static analysis of the world-controller table indicates the retail client is an online world client; the
actual encounter simulation and authored authority normally live on an activity host.

## Corrections and dead ends that should not be repeated

1. Do not force `route_commit`, route bytes, modes, lifecycle flags, or `+0x1AF00`. Native inputs and
   observe-only probes have produced reliable progress; blind forces produced transient or destructive
   states.
2. Do not treat the older "authored activator" cluster near the matchmaking rejoin code as the mission
   start switch. Decoded retail strings identify it as account-profile rejoin and join-lockout bookkeeping.
3. Do not call the previously misnamed selection-ingest function. The experiment proved it is a fireteam
   join API.
4. Do not infer a missing generic state-2 repump. The outer task did receive its second pass; the zero
   matchmaking configuration caused the inner search/provider state to be cleared.
5. Do not treat mode 8 as the solo authored mission state. Its request builder requires more than one
   established member in state 10; it is a fireteam-launch path.
6. Do not treat identity 2 as a permanent slot type. Identity handles increment per launch; in the latest
   run identity 2 simply names this launch's authored manager.
7. Do not treat kind 22 as automatically equivalent to mission-script activation. It is a
   host-reestablish/host-migration path. It now executes but does not set active.
8. Do not chase Service-7's zero 128-byte block as the route selector. Route 1 now proves it is not required
   for the client handoff achieved here.
9. `reliable_registry_decode result=rejected` on empty queues is not, by itself, evidence that authored
   content failed to decode.
10. Do not merge the old `spawner` branch wholesale. Preserve it as a client-executor reference and port
    narrow pieces only after the host/client strategy is chosen.

## Current code that matters

- `Sunrise/src/middleware/bap/matchmaking/response/matchmaking_response_encoder.cpp`
  - `encode_configuration()` emits the nested field-4 lane and bubble policy.
- `Sunrise/src/middleware/gameplay/group/session_messages.h/.cpp`
  - defines the 136-byte `HostReestablish` body and native NetAddr serialization.
- `Sunrise/src/server/gameplay/group/group_host.cpp`
  - queues kind 22 after activity-host and player publication, scoped to the opening mission.
- `Sunrise/src/client/hooks/bootflow/activity_script_upstream_probe.cpp`
  - records route lookup/commit/update, authored manager start, mode setter, table state, and direct
    `mgr_activate` before/after values.
- `Sunrise/src/client/hooks/bootflow/activity_schema_decode_probe.cpp`
  - contains the bounded service-7 response/callback investigation.
- `Sunrise/src/client/hooks/retail_log/retail_log_enqueue_observer.cpp`
  - captures decoded retail log sites and stacks used to name the native matchmaking stages.
- `ghidra_scripts/DecompManagerActiveWriters.java`
  - proves the sole `+0x1AF00 = 1` writer and the kind-22 caller.
- `ghidra_scripts/TraceMatchmakingProviderEntries.java` and related scripts
  - document the field-4 provider/lane shape and consumers.

## Next steps

### Priority 1 - Explain success-without-active inside +0x177E940

This is the narrowest next question and should be answered before changing server data.

1. Decompile the full control-flow graph of `+0x177E940`, not only the active-field neighborhood.
2. Enumerate every return-true site and identify the exact predicates separating the observed return from
   the basic block containing the sole write at `+0x177F156`.
3. Map those predicates to named manager fields, the 0x18-byte record argument, and the 136-byte decoded
   kind-22 payload.
4. Add bounded observe-only markers at the minimum number of validated basic blocks needed to identify the
   path. Prefer branch-entry markers or helper-call recorders over modifying an instruction.
5. Extend the direct recorder with the fields used by those predicates, not a broad memory dump.

Success criterion: one run tells us exactly why `result=1` and `active_after=0` coexist.

### Priority 2 - Recover and populate the two kind-22 identity blocks

The current server explicitly sends the 128-bit and 144-bit blocks as zeros. They are the largest known
difference between the experimental packet and a real host-reestablish packet.

1. Follow the kind-22 reader's copies of both blocks into the activation function and identify comparisons
   or lookups that use them before the active write.
2. Compare their shape with the membership identity already encoded by Sunrise: account SOID, character or
   opaque SOID, machine/member key, join identity, and secondary opaque/FTID lanes.
3. Fill only fields whose mapping is statically demonstrated. A/B the 128-bit and 144-bit blocks separately
   if their roles remain independent.
4. Keep `sessionId`, `machineId`, and NetAddr unchanged during this test because the manager lookup and
   transport already succeed.

Success criterion: the direct recorder observes either the sole writer or `active_after=1`. A return value
of 1 alone is no longer sufficient.

### Priority 3 - Trace the first missing downstream event after mode 5

The main mission goal should not wait entirely on the host-migration interpretation.

1. Snapshot the authored manager immediately after mode 5: active `+0x1AF00`, selected `+0x87C`, registered
   `+0xE93C`, lifecycle fields, member table, and definition pointer/state/enabled/pending.
2. Trace the first mode-5 consumer that should publish a component candidate, identity definition, activity
   event, authority object, or script receiver.
3. The current log repeatedly shows component registration/reuse while active remains 0 and the authored
   component-dispatch candidate remains 0. Find the first predicate that rejects identity 2 rather than
   tracing every later empty loop.
4. Treat `identity_enable` as one diagnostic signal, not as an assumed universal prerequisite. There are
   separate paths for enabling a definition and for activating an existing manager.

Success criterion: identify one exact missing host-provided record/event or one exact client predicate after
mode 5.

### Priority 4 - Decide the execution architecture after the boundary is measured

If the manager becomes active but the mission still has no encounter execution, choose deliberately:

- Faithful route: make Sunrise behave as the activity host and publish the authored authority, encounter,
  script, objective, roster, and cinematic state the online client consumes.
- Client-executor route: port the entity spawner and related executor from the preserved `spawner` branch,
  then solve AI authority/pathing separately.

Static world-controller analysis says there is no dormant local activity-host controller in the normal
client process. That makes host-side simulation the more faithful architecture, but the spawner remains a
useful experimental tool and fallback.

### Priority 5 - Keep the evidence baseline reproducible

For each test:

1. Confirm the live DLL hash matches the build output.
2. Start from a fresh game process and use only the Homecoming override.
3. Keep force hooks disabled; run recorders observe-only.
4. Correlate the route, manager, mode, activation, and downstream fields by manager pointer and session id.
5. Archive the log before the next run because Sunrise rotates `sunrise.log` to `sunrise.log.old`.
6. Change one server field family or one recorder boundary per run.

## Reproduction and payoff grep

1. Launch Destiny 2, sign in, and reach orbit.
2. Open Sunrise with Insert.
3. Enable the activity override for `mission_towerfall`.
4. Launch the Farm/Homecoming opening so the native 282 -> 266 correction occurs.
5. Wait through map/banner load and at least 25 seconds of activity-host traffic.
6. Close the game before deploying another DLL.

Useful grep:

```text
activity_matchmaking_lane_policy_materializer
activity_script_route_descriptor.*route=1
activity_script_route_commit.*route=1
activity_script_manager_start.*route=authored
activity_script_manager_mode_set
host_reestablish
activity_script_manager_activate
activity_script_identity_enable n=
active_after=1
definition_enabled=1
```

## Final handoff statement

We are past the old "authored route never happens offline" wall. In the newest run the client itself
selected and committed route 1, created a fresh manager through the authored starter, entered mode 1 with
the exact native 0x17F8 payload, and advanced the same identity-2 manager through 1 -> 2 -> 4 -> 5. The
server's 136-byte kind-22 message also reached the verified activation function. The remaining immediate
problem is now sharply bounded: `mgr_activate` returns true without executing the only native write that
sets `manager+0x1AF00` to 1, and no authored mission scripts or encounters start. The next run should explain
that internal branch and recover the two currently empty identity blocks before any state is forced.

