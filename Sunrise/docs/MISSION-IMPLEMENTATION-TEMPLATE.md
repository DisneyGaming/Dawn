# Mission implementation template

Reusable Sunrise mission brief, implementation record, and acceptance checklist. Updated 9 September 2026 with lessons from Gateway, Omega, and Beyond Infinity.

Copy this file to your mission's implementation document and fill in the brief. Unknown native IDs can remain **UNKNOWN** at kickoff; the implementer must recover them. Remove sections that do not apply. Reference documents are evidence; the user's current request determines scope and authorization.

## 1. Mission brief

- **Mission / content version / variant:** [fill in]
- **Mission identifier:** [snake_case]
- **Scope:** [start through ending, including any destination afterward]
- **Difficulty / player count:** [fill in]
- **Reference walkthrough:** [URL or accessible file; complete mission time range]
- **Transcript / subtitles / extra clips:** [locations; timestamp convention]
- **Current state:** [new / partial / playable with known failures]
- **Already working and to preserve:** [list]
- **Required mechanics and fidelity:** [encounters, scenes, dialogue, devices, travel, objectives]
- **Checkpoint / death / retry requirements:** [fill in or explicitly unresolved]
- **Deferred or excluded content:** [list; do not silently omit missing scenes]
- **Live work requested:** [inspection / targeted memory repairs / guided playthrough]
- **Delivery requested:** [source / tested patch / installation / live acceptance]
- **Session authorization and constraints:** [record what the user actually authorized]

### Starting request

> Implement this mission using the current Sunrise Lua executor and existing native services. Inspect the current checkout before proposing new infrastructure. Review the complete reference segment and recover this mission's native bindings. Reconstruct its scenes, dialogue, encounters, devices, transitions, and ending within the scope above. Test uncertain behavior live when requested. Carry accepted live repairs into source and deliver the requested validated package with installation and rollback evidence. Keep assumptions and untested behavior explicit.

## 2. Baseline and evidence

- Workspace / branch / commit / existing uncommitted work: [record and preserve]
- Game executable path / SHA-256: [fill in]
- Installed DLL / matching PDB / SHA-256: [fill in]
- Installed mission script / SHA-256: [fill in]
- Package directory / game build: [fill in]
- Installed candidate / source archive / installation receipt: [fill in]
- Launch route / activity override: [fill in]
- Log and research directory: [fill in]

Read [the reconstruction guide](NEW-MISSION-RECONSTRUCTION-GUIDE.md), [Lua authoring](LUA-MISSION-AUTHORING.md), and [shared services](UNIVERSAL-MISSION-SERVICES.md), then verify their claims against current source. Older documents contain historical gaps and build counts. Use matching receipts and current code to establish what is installed.

Label claims as **VIDEO**, **PACKAGE**, **LIVE READ**, **LIVE INTERVENTION**, **USER CONFIRMED**, **OFFLINE TEST**, or **ASSUMPTION**. Record the file, timestamp, build, and run where applicable. An offline pass and a visible in-game result establish different things.

## 3. Architecture: reuse before extending

We already execute native Scene graphs. Do not rebuild native scene execution merely because another mission needs different actors or event IDs.

- **Lua:** story order, dependencies, dialogue selection, encounter scheduling, objectives, permitted cue delays, and completion conditions.
- **Mission bindings/catalog:** recovered identities, placements, cast references, trigger geometry, native inputs/outputs, tactical assignments, device values, and observation policies.
- **Shared services:** execution, dialogue queuing, scene orchestration, object initialization, destructibles, population readiness, event clocks, objectives, and lifecycle ownership.
- **Engine adapters/hooks:** authenticated native observations and native operations on the correct thread. Forward these to the relevant services.
- **Explicit mission repairs:** narrowly identified exceptions with evidence, ownership checks, and retirement behavior.

Inspect the existing code in [the shared service directory](../src/state/activity/coo/), especially scene_service.h, scene_orchestration.h, native_scene_authority.h, native_scene_cast_authority.h, dialogue_service.h, object_service.h, population_service.h, event_timeline.h, and lifecycle_service.h.

Use [Gateway](../scripts/gateway.lua) for native box ownership and authored progression; [Beyond Infinity](../scripts/beyond_infinity.lua) for traversal, scene/dialogue separation, native portals, and repeated Forest visits; and [Omega](../scripts/omega.lua) with its native Forest adapter for generator execution. Recover the new mission's own identities and authority schema. Omega retains older phase constraints; those are not a default story contract for new missions.

### Capability decision record — repeat for each missing operation

- Desired native behavior: [fill in]
- Existing service and hook inspected: [file / function]
- Missing capability or binding: [specific gap]
- Chosen implementation: [existing capability / shared extension / explicit mission repair]
- Evidence and affected missions: [fill in]
- If adding an interception: [target, executable identity, ABI, original-call forwarding, owner checks, teardown]
- Regression checks: [fill in]

Count actual intercepted engine addresses separately from helper files. Existing omega_ and gateway_ hook filenames also contain shared routing. Prefer extending a shared boundary over attaching another hook for the same operation. Keep new mission data and exceptions grouped with the mission where the existing layout permits it.

## 4. Reference reconstruction and native map

Review the entire requested video segment, including the lead-up to each action and the closing dialogue after apparent escape. Mark loading cuts, edited footage, off-camera teammates, and uncertain timings. A transcript establishes spoken content; it does not identify a native bank row or prove the trigger.

Recover the activity, scenario, launch descriptor, region/bubble, spawn set, and authority schema independently. Confirm player control and actual landing geometry before diagnosing downstream scripting.

### Beat record — repeat in observed order

- Beat ID / phase / location: [fill in]
- Video timestamp / objective text: [fill in]
- Player action or native event: [fill in]
- Preconditions: [what truly must already have happened]
- Immediate visual action: [scene, actor, device, environment, encounter]
- Dialogue and owner: [native Scene or shared queue]
- Native inputs and expected outputs: [bindings and evidence]
- Progression gate: [specific authenticated event]
- Early arrival / repeated crossing / return visit: [behavior]
- Reset / death / cleanup: [behavior]
- Evidence, unknowns, and acceptance status: [fill in]

### Native binding record — repeat for each important source

- Alias / kind: [fill in]
- Package / registry / definition / type / slot: [fill in]
- Definition class and runtime component class: [record separately]
- Placement / coordinate space / native owner: [fill in]
- Source generation / salted entity or component identity: [resolution method]
- Requested operation / authentic acknowledgement: [fill in]
- Extract or capture proving the mapping: [location]

Do not copy numeric constants between missions because objects look similar. In Beyond's Forest, definition class 80804EF6 and runtime class 80804EF7 differed; checking the definition class against the live component prevented generation entirely.

## 5. Triggers, native scenes, and dialogue

### Trigger record

- Native volume identity and geometry: [polygon, height, coordinate space]
- Observation mechanism: [native callback / sampled recovered native volume]
- Required semantics: [current occupancy / retained crossing / exit / once per visit]
- Arming prerequisites and visit owner: [fill in]
- Early-entry retention and reset policy: [fill in]

Use actual mission volumes when recovered. Beyond currently evaluates recovered native geometry against player position; that differs from an engine trigger callback. Name the mechanism accurately. Account for concave volumes and region changes. A bounding-box center can lie outside the actual trigger.

Latch valid crossings while other prerequisites finish so a fast player need not walk backward. For a return destination, require current occupancy or evidence belonging to the new visit. An old first-pass observation must not complete the second pass.

### Scene record

- Native Scene descriptor / selector graph / definition: [fill in]
- Complete cast in authored order: [actors, objects, markers, sources]
- Scene-owned versus externally owned sources: [fill in]
- Start volume and native input event: [fill in]
- Child-scene handoffs and cast parameters: [fill in]
- Speech owner and speech milestones: [fill in]
- Animation/cue outputs and completion gate: [fill in]
- Lifetime / cleanup / replay behavior: [fill in]

Preserve cast order, including non-actor references. Avoid independently spawning a source already owned by the Scene or activating alternative variants that share the same cast. A scene request or a spoken line does not prove its actor appeared and animated.

For a disappearing actor, inspect the initial scene, hold scene, main child, parameter binding, weak handle/salt, and ownership at the handoff. Validate field offsets against the actual child definition. Beyond's disappearance near "But that was before you" led to a conditional missing-cast recovery; its combined-patch visual acceptance was still pending. Do not treat that recovery as a universal respawn rule or override later authored removals.

### Dialogue record

- Speaker / exact phrase used to identify the line: [fill in]
- Bank / row / selector / variant: [recovered mapping]
- Owner: [native Scene OR shared dialogue service]
- Submission trigger and deduplication scope: [run / visit / scene generation]
- Ordered predecessor, if queued: [fill in]
- Start evidence: [native submission or native speech start]
- Completion evidence: [speech-node finish / acknowledged bank duration / documented estimate]
- Retire/reset behavior: [fill in]

Assign one playback owner to each exchange. Inspect native child graphs before adding global dialogue; Beyond's Future duplicates came from requesting speech already owned by a native child. Two reflections can animate while only one owns their shared line.

Separate queue acceptance, native submission, speech completion, and whole-Scene completion. A scene can retain an idle branch after speech ends. A timer started at request time can expire before playback starts. When only duration-based completion is available, record that limit explicitly.

### Traversal scenes that must keep up with the player

This is a dependency sketch, not executable Lua or new capability names:

~~~text
cross platform volume -> start native platform scene -> enqueue its dialogue
cross stairs volume   -> start native stairs scene   -> enqueue its dialogue
cross upper volume    -> start native upper scene    -> enqueue its dialogue

dialogue queue: platform -> stairs -> upper -> corridor exchange
required final speech finished + entrance reached -> release next section
~~~

Animation prerequisites should contain the necessary native readiness and movement events. Wait for earlier speech only when the intended performance requires it. Keep queued speech ordered across phase handoffs and deduplicated across repeated crossings.

Separate native speech from animation only after inspecting graph edges and proving that muting the selected speech callback cannot suppress required animation, actor, or progression actions. Beyond's .silent and .queued bindings are selected mission capabilities, not automatic support for every Scene. See [the Well speech verifier](../../tools/coo/verify_beyond_well_speech.py).

Test walking quickly through several scene volumes while the first exchange is still playing. Confirm every intended scene appears, voices remain ordered, and the final reveal still gates the next section. Retire completed one-time scene inputs so a region reload cannot replay the opening presentation.

## 6. Devices, plates, destructibles, and clocks

### Mechanism record

- Native sources, entities, controllers, and links: [fill in]
- Preparation -> creation -> binding -> starting-state acknowledgement: [evidence]
- Initial visibility / protection / beam states: [fill in]
- Occupied / charging / completed / destroyed states: [native values for each controller]
- Actual native timer and clock source: [fill in]
- Interrupted charge versus latched completion: [fill in]
- Authenticated destruction receipt and resulting gate changes: [fill in]

Carry forward these Well lessons:

- Box visibility, protective barrier, each beam, and the plate's rising holographic border are separate controls. A beam appearing is not evidence that the plate VFX works.
- Check the intended initial state before the first interaction. Do not require stepping off and back on to initialize the effect.
- Recover device modes from native conditions. A generic "1 = on" assumption failed for both plate and portal effects.
- Verify that the native gameplay clock advances before changing animation controls to diagnose frozen progress. Observe native timer progress and completion; do not write a completion flag to stand in for charging.
- Reset interrupted charges according to the mission policy. Preserve a completed charge across stepping off when the puzzle is intended to stay solved. The box barrier must not reappear after that completion.
- Let real native box destruction open the wall. Occupancy or a guessed delay cannot establish destruction.
- Call animation/device functions from the verified engine thread with the correct ABI. An authority callback that binds an owner is not automatically a suitable animation execution point.

For new clock/protocol fields, verify native units, epoch, endianness, bit alignment, and reset semantics against the original decoder. Zero values can hide wrong encoding. Beyond's incorrect timestamp byte order caused heartbeat flooding and Anteater; the disconnect near box destruction was not enough to identify the cause. Capture earlier clock/network events as well as the final error.

## 7. Portals, receiving areas, and generated routes

### Portal record — repeat for every direction

- Source region and exact contact barrier: [fill in]
- Native transporter and destination: [fill in]
- Visual frame / core / effect / device modes: [fill in]
- Associated reflection or scene: [fill in]
- Placement / height / orientation / floating animation: [native evidence]
- Reveal prerequisite: [specific dialogue completion or native cue]
- Receiving volume and observation freshness: [fill in]
- Retirement and return-trip behavior: [fill in]

Validate transport and presentation independently. Beyond had a working escape transporter with invisible effects, and a working Past return portal with incorrect appearance. Recover the effect's native mode instead of forcing position 1; one Beyond portal activates around position 0.1. These values are examples, not defaults for another portal.

A region loading boundary is not portal contact. Let the verified native portal perform its transport and observe arrival in the destination. Do not add a host teleport just because a load zone was entered. Test waiting beside the barrier without touching it, then crossing it.

When a line says "Go" or "Run," identify the intended portal reveal cue or speech boundary and release the portals there. Keep estimated delays tied to an acknowledged event and label them. For an introductory reveal, preserve the final speech gate before exposing the next section. An entry tutorial may belong to the actual jump into that section rather than standing at its overlook.

### Generated route record — repeat for EACH pass

- Generator / worker / runtime sensor identity: [fill in]
- Configuration, seed ownership, race, encounter recipe: [fill in]
- Fixed start platform and fixed destination: [native placements]
- Entrance side / row / height: [fill in]
- Exit side / row / height: [fill in]
- Coordinate transform / grid dimensions: [fill in]
- Native doors and Daemon ownership: [fill in]
- Start connection acceptance: [evidence]
- Final generated node -> fixed endpoint acceptance: [evidence]
- Revisit observations / reset / generation cleanup: [fill in]

Reuse Omega's recovered generator mechanics while supplying this mission's recipe and endpoints. Default prefab endpoints can produce a valid route to the wrong place. Beyond required separately verified north-to-west and west-to-east routes. Those directions are specific to Beyond.

Walk and clear each complete route, including the final connection. An entrance that connects does not establish a correct exit. Preserve native encounter and Daemon door control. Enabling a gate flag is not the same as generating missing geometry.

For a live rebuild, capture the last generated node and fixed endpoint first. Have the user move to the fixed platform and confirm their position before replacing generated geometry underfoot. Keep the two passes' recipes, observations, and acceptance records separate.

## 8. Encounters, environmental performances, and ending

### Encounter record

- Authored population sources / variants / count evidence: [fill in]
- Required versus optional deaths: [fill in]
- Health / AI / tactical group / behavior intent: [fill in]
- Shield or immunity collection and effect: [fill in]
- Spawn/reveal cue / combat activation / cleanup: [fill in]

Spawned enemies are not necessarily combat-ready. Verify native health, AI ownership, tactical assignments, and intended behavior. A shielded escape ambush can require active AI without requiring kills. Do not compensate for inactive AI by changing actor limits or enabling every source.

Ensure collection effects target every intended actor, including later members of the authored population. Visual shielding, immunity, AI readiness, and creation need separate checks.

For environmental performances, map arrival dialogue, scene input, structure creation, structure motion, subsequent speech, actor lifetime, and the exit portal individually. Beyond's Past initially had scenery but lacked the arrival exchange, construction events, return reflection/line, and portal. Reaching the region did not prove the vignette was wired.

Record a separate dialogue policy for each visit to a repeated area. Beyond's second Forest traversal requires its return exchange, and must not replay the first Forest introduction or leave its old Scene inputs armed.

### Ending contract

- Escape transport and receiving corridor: [fill in]
- Actual final hallway/exit trigger: [fill in]
- Dialogue at arrival versus after leaving the hallway: [fill in]
- Full final exchange completion evidence: [fill in]
- Final objective / mission-complete publication / next destination: [fill in]
- Cleanup after the required presentation: [fill in]

Verify the final trigger's actual region and coordinate space. Beyond's "Ikora! You there?" exchange was waiting on the wrong ending trigger. Escape transport, hallway exit, final speech, and mission completion are separate milestones. Do not finish the mission early and cut off the closing exchange.

## 9. Live experiment record and persistence

Repeat this record for each targeted repair:

- Hypothesis and expected visible/audible result: [fill in]
- PID / process creation time / game and DLL hashes / module bases: [fill in]
- Run / generation / salted owner / component identity: [fill in]
- Resolved address / field type / original bytes: [fill in]
- Smallest proposed change and supported execution thread: [fill in]
- Capture and user position before the change: [fill in]
- Actual native result and user observation: [fill in]
- Undo/retirement under the same valid owner: [fill in]
- Permanent source change and fresh-build acceptance: [fill in]

Re-resolve identities immediately before writing. Discard addresses after process restart, unload, owner retirement, or generation change. Preserve raw captures with the interpretation. A temporary memory hold can be overwritten next tick and is not a persistent fix. Never restore bytes into an allocation whose owner changed.

For a scene replay, ask the user to be tabbed in, then replay the identified scene and give one concrete visual/audio check. A missed foreground playback is inconclusive. "Dialogue submitted" in the log does not prove the user heard it, and speech playback does not prove the actor animated.

When a user reports an error code, preserve the full relevant log before rotation and inspect earlier clock, authority, ownership, and scheduling failures. Avoid repeating a failing intervention without revising the hypothesis.

Native memory experiments require their own evidence and cleanup. Lua loads into an immutable definition once per process; editing its source does not update the running mission. Installing new compiled bindings or normal C++ changes requires a matching DLL and restart. Specialized live patches do not imply general hot reload.

## 10. Tests, package, install, and acceptance

Implement one playable section first, prove its native ownership and completion, then extend the route. Test meaningful failure paths: early/repeated crossings, queued voices, stale owners, interrupted versus completed charge, repeated visits, scene handoffs, real death, reset, and terminal cleanup. Use independent captures/decoders where binary layout is involved.

### Candidate checklist

- [ ] New mission selection, native routing, source/project items, and script registered.
- [ ] Focused mission and affected shared/native tests pass.
- [ ] New suites registered in tools/coo/verify_lua.py.
- [ ] Script included in tools/coo/package_lua.py and installer/rollback file lists.
- [ ] Installer result expectations agree with the full runner; no check bypasses.
- [ ] Fresh candidate directory and unchanged source manifest through validation/package.
- [ ] Full configured Debug/Release suites and both DLL builds pass.
- [ ] Matching source archive, payload hashes, symbols, and previous installation preserved.
- [ ] Game closed before DLL replacement; installed files verified against the manifest.
- [ ] Log identifies the intended mission, Lua script, and fingerprint after restart.

As checked on 9 September 2026, the runner has 18 suites in Debug and Release plus both DLL builds, giving 38 results, and packaging includes four mission scripts. Inspect current lists before adding the next mission; these are baseline counts, not permanent requirements.

Run from the workspace root, replacing the candidate name and using the configured Python interpreter:

~~~powershell
python tools/coo/verify_lua.py --out build/coo/<mission>-<candidate>
python tools/coo/package_lua.py --validation build/coo/<mission>-<candidate>
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/<mission>-<candidate> -ValidateOnly
# Close Destiny before replacing its DLL.
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/<mission>-<candidate>
~~~

A focused --project run supports development; packaging requires complete configured validation. Use a new candidate directory for later changes. Documentation is included in source hashing, so finish documentation edits before freezing the candidate.

### Fresh-process playthrough checklist

- [ ] Correct launch, landing, player control, and initial device/VFX states.
- [ ] Native cast appears and animates at the intended movement triggers.
- [ ] Dialogue plays once per intended visit, in order, including fast traversal.
- [ ] Required speech completes before gated reveals or transitions.
- [ ] Charge interruption, retained completion, and real destruction behave correctly.
- [ ] Every portal has correct visuals, position, contact behavior, and destination.
- [ ] Every generated route connects at BOTH ends after clearing native encounters.
- [ ] Environmental performances and subsequent portals run in the intended order.
- [ ] Enemies have the required AI, tactical behavior, shields, and death policy.
- [ ] Actor handoffs preserve the intended performance and cleanup.
- [ ] Full ending exchange finishes before mission completion.
- [ ] Requested retry/checkpoint behavior works without stale observations.
- [ ] Full run completes without debug forcing, memory holds, or fabricated receipts.

## 11. Delivery record

- Source changes and launch instructions: [fill in]
- Candidate / package / source archive: [fill in]
- Tests and failures: [fill in]
- Installation receipt / installed hashes / backup: [fill in]
- Accepted live segments and exact tested builds: [fill in]
- Live interventions used during development: [fill in]
- Fresh unmodified full-run result: [fill in or NOT YET TESTED]
- Assumptions, fidelity gaps, deferred content, and next focused check: [fill in]

State separately what is implemented, what passed offline checks, what was accepted through live intervention, and what passed in the installed fresh process. Beyond's combined patch preserved live-confirmed Forest connections, while its new Well queue timing and conditional Future actor recovery still required fresh live confirmation. Carry that distinction into the next handoff.

### Beyond Infinity evidence to consult

- [Current mission Lua](../scripts/beyond_infinity.lua) and [finale notes](BEYOND-INFINITY-FINALE.md).
- [Well clock and byte-order investigation](BEYOND-INFINITY-IMPLEMENTATION.md); sections describe historical candidates, so check dates and receipts.
- [Well speech graph verification](../../tools/coo/verify_beyond_well_speech.py) and [native actor fallback emulation](../../tools/coo/verify_beyond_future_cast.py).
- [Combined patch evidence](../../build/coo/beyond-infinity-combined-fix-20260909/combined-fix-evidence.json).
- [Both Forest connections accepted live](../../build/coo/beyond-infinity-endpoint-live-20260909/live-acceptance.json).

Evidence under build/ may be local rather than versioned. Preserve or provide the matching archive when handing this template to someone without the workspace. These are research examples; verify the current installed candidate instead of assuming this historical patch is still active.
