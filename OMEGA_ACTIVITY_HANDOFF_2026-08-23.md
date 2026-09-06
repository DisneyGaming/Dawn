# Omega activity/runtime handoff — 2026-08-23

## Executive summary

Omega (`mission_scot`) is no longer blocked at roster discovery or basic authored-object initialization. Sunrise can load the measured six-group Omega roster, the client creates Destiny's native mission/runtime objects, objective markers work, authored trigger volumes report sensor events to the server, and the opening scene/Ikora path can be driven through authoritative state.

The remaining concrete blocker is the transition through the Mercury portal into the Infinite Forest. The correct authored entrance sensor is detected and the server advances Omega to script state 4, but Destiny's native route controller never arms its active route bit. Consequently, the route callback and publication stages never run, no teleport/transition packet appears, and the destination bubble is not loaded.

The latest deployed DLL adds an observation-only probe around the native lifecycle-controller accessor. It has not yet been exercised: the current log predates that DLL. The immediate next move is to run the latest build, reproduce the portal sequence, and use the new caller inventory to locate the native event or setter that should arm the route.

This is not a finding that Omega must be rebuilt from scratch. The client still contains the authored scenes, dialogue, placements, sensors, mission components, and route machinery. Sunrise needs to reconstruct the missing authoritative host-side orchestration that tells those client components when to change state.

## Current workspace and live build

- Workspace: `C:\Destiny 2 Development`
- Source tree: `C:\Destiny 2 Development\Sunrise-src\Sunrise`
- Live DLL: `C:\Destiny 2 Development\bin\x64\steam_api64.dll`
- Live DLL size: `12,154,880` bytes
- Live DLL SHA-256: `5DE45C7022E7945172B2C5FFDDE9EF9689B8949E7080FE09B884CC87349C93BA`
- Live DLL timestamp: `2026-08-23 20:12:34 -04:00`
- Active log: `C:\Destiny 2 Development\bin\x64\Sunrise\logs\sunrise.log`
- Last inspected log timestamp: `2026-08-23 20:08:27 -04:00`

The runtime settings are:

```json
"roster_force_authored": true,
"seed_authored_sensors": false
```

`roster_force_authored` enables the measured Omega compatibility roster. `seed_authored_sensors` remains disabled, so the relevant events come from the client detecting actual authored conditions rather than Sunrise fabricating them.

Important: the live DLL is newer than the active log. No conclusions about the newest lifecycle-accessor probe can be drawn until another run is captured.

## What is proven to work

### 1. The measured Omega roster loads

When all four Omega-specific authored keys are discovered:

```text
82FB58B7
D00142CF
BA5F26EF
F7A6CE7F
```

Sunrise publishes the exact six-group roster measured from the run in which Ghost navigation and objective markers worked:

```text
4786C0E0  player/runtime group
29D7B029  type-19 group
82FB58B7  directive/objective, dialogue, and music sensors
BA5F26EF  authored mission group
D00142CF  Ikora, gate, scene, and opening-volume group
F7A6CE7F  authored dialogue/mission group
```

The successful layout is six groups and 57 objects. If discovery does not find the complete four-key Omega set, the publication code removes the incomplete forced subset and falls back to dynamic discovery. Other destinations remain dynamic.

Relevant files:

- `Sunrise/src/client/content/scenarios/scenario_roster_groups.cpp`
- `Sunrise/src/client/content/scenarios/scenario_roster_build.cpp`
- `Sunrise/src/client/content/scenarios/scenario_roster_publish.cpp`
- `Sunrise/src/core/settings/client/definition.h`
- `Sunrise/src/core/settings/client/client_settings_parser.cpp`

### 2. Destiny's native mission/runtime layer is present

The client constructs its native mission director, activity-script manager, scenes, sensors, and other authored runtime components after roster registration. The activity-script manager is Destiny client code; Sunrise did not recreate that manager. Sunrise's additions are hooks, probes, roster compatibility, server state handling, and authority publication around it.

This supports the main working model: much of the mission content survived in the packages, while the original authoritative Destiny host logic did not.

### 3. Authored sensors reach the server

The client detects authored trigger-volume conditions and sends activity message type 6 to the host. The server can parse the object identity, component slot/index, sense value, destination binding, activity epoch, and session membership.

The original server path only acknowledged type 6 and logged `host_action=none`. The current source has evolved into a bounded Omega opening state machine: exact known opening and forest sensors can latch state changes and queue authority publication. Generic or unmatched sensor reports remain observation-only.

### 4. The opening state and scene handoff can progress

The current Omega-specific server logic recognizes:

- The authored opening monitor in group `D00142CF`, component type 30/index 20.
- The authored scene handoff in group `D00142CF`, component slot 43/index 1.
- The authored Infinite Forest entrance in group `D00142CF`, component type 30/index 24.

The opening transition can settle to state 2. The scene handoff waits for the native scene to retire rather than manually injecting a scene request. The Infinite Forest entrance can settle to state 4 and begin the route investigation.

The exact code is primarily in:

- `Sunrise/src/server/bap/encrypted/activity_message/activity_message_route.cpp`
- `Sunrise/src/server/bap/encrypted/activity_roster/activity_roster_report.cpp`

### 5. The Ikora/scene path uses client-authored content

Ikora, the gate, and the opening scene are authored client objects from the restored roster. Their cast and scene behavior came from Destiny's native scene machinery after Sunrise supplied the expected authority state. Sunrise did not rebuild the animations, cast, dialogue, VFX, or placements.

The observed T-pose/anchor and VFX behavior is a separate presentation problem. In this mission, “scene 2” was identified as the VFX-related scene. That issue has intentionally been excluded from the portal-transition investigation.

## The progression model we have established

The evidence supports this pipeline:

```text
authored packages and roster
        ↓
Destiny client constructs authored runtime components
        ↓
authored sensor condition becomes true
        ↓
client sends type-6 sense update to Sunrise host
        ↓
host validates the event and commits mission state
        ↓
host publishes authoritative component/script state
        ↓
Destiny client activates its native scene, gate, spawner, route, dialogue, etc.
```

This explains why simply restoring the roster fixed markers but did not finish the mission. The roster supplies the authored pieces; it does not replace the missing authoritative progression logic.

## What Sunrise is and is not manually doing

Sunrise is currently mapping exact authored sensor events to exact authoritative mission states. That is server orchestration, not replacement of the client content.

We are not planning to drive Omega by manually sending arbitrary type-5 “play this scene” messages. Scenes should activate through their authored client components after the server publishes the required state. Type 6 remains a client-to-host sensor report, not a request/reply teleport API.

The intended reusable design is a small server-side state machine or DSL:

```text
when <authored sensor> reports <condition>
and  <mission state/session guards are valid>
then advance <host mission state>
and  publish <authored component-state changes>
```

That DSL would refer to package-authored object/component identities. It would not contain replacement animation, AI, map, dialogue, or scene data.

## Portal/Infinite Forest transition findings

### Retail expectation

After Ikora opens the gateway, the player steps through the portal, enters a transition area, and then reaches the Infinite Forest. The authored entrance sensor is firing in Sunrise, and the host reaches Omega script state 4.

### Runtime result

At state 4, the client focuses on native route slot 38:

- Route 38 is current/pending and has identifier `32000`.
- Route 39 is populated, but its identifier is `0`.
- Route 39 therefore cannot safely be assumed to be “the next bubble” or the destination.
- The advertised world region remains 120.
- No type-22 transition appears.
- `teleport_state` never reaches 1.
- No new destination bubble loads.

The client route evaluator runs repeatedly, but its observed state remains:

```text
phase=0x02
flags=0x00
lane1=0
lane2=0
route_state=0
selection_timestamp=-1
```

In the 20:08 log, state 9/script state 4 is visible around line 10104; route 38 still has `q68=0` around line 10151; route 39 is reported around line 10156; the lifecycle query returns 3 around line 10165; and the route flag remains zero around line 10172. These line numbers are only anchors for that particular log and will move on the next run.

### Native route path decoded so far

Route 38 method 30 is at RVA `0xD4B870`. It performs roughly this sequence:

1. Route preparation through `0xD4D200`.
2. Mode/lifecycle application through `0xD4D360`.
3. Embedded route-driver execution through `0xD4B080`, using state at `owner + 0x40`.

The embedded driver's important owner fields are:

```text
owner + 0x48  enabled/busy-related mask
owner + 0x68  active mask
owner + 0x78  invalid/error mask
owner + 0x80  completed mask
owner + 0x88  selection timestamp/status, initialized to -1
```

The route initializer, method 18, deliberately zeroes `+0x68`, `+0x70`, `+0x78`, and `+0x80`, and initializes `+0x88` to `-1`. Some later event is therefore responsible for arming the active mask.

The embedded driver exits before callbacks or publication when the route is inactive, already complete, invalid, or otherwise disallowed. In the current run, `owner + 0x68` bit 0 stays clear, so the driver takes the inactive early-exit path. This is the immediate mechanical reason the portal transition does nothing.

### Lifecycle/controller discovery

The mode-apply function at `0xD4D360` calls lifecycle query `0x4FFD10`. That query:

1. Calls accessor `0x4FF9F0` to obtain a controller pointer.
2. Returns signed controller byte 0, unless the pointer is null or byte 0 is `0xFF`.

The observed lifecycle value is consistently `3`, both at the method-30 gate and at mode apply. It does not change after entering the portal.

The self-arm branch in mode apply sets `owner + 0x68` bit 0 only for lifecycle values 7 or 10 when the other masks are clear. The current value 3 therefore does not take that branch.

This does **not** yet prove that the correct fix is to force lifecycle 7 or 10. Two live models remain:

1. Retail changes the controller from state 3 to 7/10 through a missing native event, after which the route self-arms.
2. State 3 is normal gameplay, and an authored external activation path is supposed to set the route active bit without using the 7/10 self-arm branch.

The newest probe is designed to distinguish these models.

## Latest deployed diagnostic

The current DLL hooks the native controller accessor at `0x4FF9F0`. It records:

- Each unique native caller.
- The returned controller pointer.
- Controller bytes 0, 1, and 2.
- Controller qword at offset 8.
- A short stack sample.
- Code windows around the first unique callers.
- A dump of the accessor itself.

It is observation-only. It does not modify lifecycle state, route masks, or world state.

On the next run, search the log and analysis output for:

```text
activity_script_omega_route_lifecycle_accessor
activity_script_omega_route_lifecycle
activity_script_omega_route_lifecycle_accessor*
```

Relevant probe implementation areas:

- `Sunrise/src/client/activity/activity_script_upstream_probe.cpp`
- `Sunrise/src/client/activity/activity_schema_decode_probe.cpp`
- `Sunrise/src/client/activity/internal.h`

## Experiments and assumptions that did not solve the transition

The following paths produced no valid teleport and should not be repeated without new evidence:

- Guessing or hardcoding a next bubble.
- Assuming `lighthouse_teleport` is the correct Omega transport component based on its name.
- Treating route 39 as route 38's obvious successor.
- Activating guessed component pairs without proving their native semantics.
- Waiting for a type-22 transition that the client never reached.
- Treating the portal as a simple coordinate teleport.
- Using presentation bugs such as the T-pose/VFX anchor as an explanation for route inactivity.

The following would be unsafe or too broad as production fixes:

- Raw-poking `owner + 0x68` without identifying the owning native activation contract.
- Globally forcing lifecycle state 7 or 10.
- Synthesizing arbitrary type-22 packets.
- Sending arbitrary type-5 scene calls to imitate progression.

Those interventions might make one symptom move while corrupting route ownership, session authority, or other activities.

## What the findings mean for the full mission

The packages give us the “what”: named objects, placements, sensors, component types, scenes, dialogue, gates, encounters, and route structures. They do not directly give Sunrise the removed first-party server program that says “when this exact condition occurs, advance these authoritative states.”

Therefore, an IGN walkthrough or mission description is useful for establishing the human-visible order of events, but it is not enough by itself to identify every object/component/state value. The reliable workflow is:

1. Use the walkthrough to define the expected phase order.
2. Inventory Omega's authored objects and components from the packages/runtime roster.
3. Observe which authored sensors fire at each phase.
4. Decode the client consumer for the corresponding authority state.
5. Add the smallest guarded host transition.
6. Verify the native client performs the authored result.

If the route activation contract is solved, the same method can be used to wire later Omega phases. It will not make every teleport mechanism universal: bubble streaming, same-map relocation, man cannons, activity migration, and scene-driven repositioning may use different native component families. The reusable unit is a decoded route/component family plus a generic authoritative-state adapter.

## Immediate next steps

1. Launch the latest DLL and reproduce the sequence through the opened portal.
2. Confirm the run contains `activity_script_omega_route_lifecycle_accessor` records; otherwise verify hook installation and executable compatibility.
3. Inventory the unique accessor callers and inspect their dumped code windows.
4. Identify writers or event handlers associated with the same controller, especially paths that assign values 7/10 or activate route ownership while state 3 remains valid.
5. Trace the discovered native activation entry point back to the authored component or authority field that invokes it.
6. Add a controlled experiment through that native helper/contract—not a raw memory poke.
7. Re-run and require the success signals below before generalizing the implementation.

Success criteria for the portal experiment:

```text
owner + 0x68 bit 0 changes from 0 to 1
embedded route driver reaches its callback
route lane/selection state changes
native route publication occurs
world controller begins a real transition
the transition area or Infinite Forest bubble loads
```

After that proof, replace the Omega-only branch with a small data-driven rule format that binds authored sensor identities to validated mission transitions and native component authority updates.

## Suggested handoff conclusion

The project is not at “write Omega from scratch.” It is at “reconstruct the missing host conductor while reusing the client-authored orchestra.” The opening sequence demonstrates that model working. The portal has now been narrowed to a specific inactive native route bit and its missing activation contract. The newest build is intended to reveal who owns that contract; running and analyzing it is the highest-value next action.
