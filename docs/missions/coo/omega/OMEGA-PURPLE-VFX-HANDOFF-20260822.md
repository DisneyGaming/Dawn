# Omega Ikora Purple VFX Debugging Handoff

Date: 2026-08-22

Scope: only the incorrect Ikora purple aura/beam visuals in the opening of `mission_scot` / Omega. This is not a general Dawn networking, matchmaking, authored-route, spawner, objective, gate-travel, or full-mission report.

This document is intended to be attached to a new Codex chat together with the current source and latest `dawn.log`. It deliberately separates confirmed runtime evidence from visual inference and untested hypotheses. Earlier statements in the chat that a particular change was “100%” the fix were not justified. The VFX root cause is narrowed substantially but is not solved yet.

## Executive summary

The real Omega opening scene now runs far enough to create Ikora, play her authored animation, and play her voice line. The remaining visual defect is that an additional static/frozen/T-pose-like Ikora representation is visible, and the purple aura/beams appear spatially associated with that wrong representation instead of the animated/final Ikora. The VFX timing follows the scene, but its spatial binding/presentation is wrong. The effect and the unwanted representation have often disappeared together at scene completion.

The latest run proves all of the following:

- The two purple scene events exist and execute.
- Their authored effect references are `0x80B9FDBE` and `0x80C220EB`.
- Both use scene `0x80FCCE87` and transform selector/index `1`.
- Selector `1` is valid and continuously updated during the animation; this is not a zero or missing transform.
- The selector-1 transform is read through the resolver's `direct_bank` path.
- In that path, the callback's object argument is explicitly not consumed.
- A temporary late rewrite of the callback context object from factory Actor 1 to factory Actor 2 definitely executed for both purple events, and was restored safely.
- That rewrite did not change the visuals because it was made after the real transform source had already been selected; the direct-bank path ignored it.
- The exact 32-byte selector-1 transform also appears inside the component captured under factory sequence 2 with definition `0x815B84E3`, at component offset `0x210` (`528` decimal; position begins at `0x220` / `544` decimal).
- The same `0x815B84E3` component contains the full Actor-1 handle at offset `0x43C`.
- The native writer that refreshes bank entry 1 is source kind `2`, output range `1..1`, source index `1`, flags `0x3C`, and no direct cast slot (`cast_index=-1`).

The strongest remaining candidate is therefore upstream of the type-23 callback: source-kind-2's binding/resolution for bank entry 1. The next controlled test was going to swap the `0x815B84E3 + 0x43C` handle from current Actor 1 to current Actor 2 only while the native source writer generates bank entry 1, then immediately restore it and compare the resulting 32-byte transform. That experiment has **not** been implemented, built, deployed, or tested.

## Critical current-state warning

The source tree and deployed DLL do not currently represent the same experiment.

- Current source file:
  `C:\Destiny 2 Development\Dawn-src\Dawn\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp`
- Source timestamp: `2026-08-22 01:15:23`
- Source SHA-256: `CF7452CDF6F2172C8DC9133488204E5049DE45F991CF635CB2E310A03E620F9A`
- Git status: the entire file is untracked (`??`), so ordinary `git diff` will not show its history.
- Current source has the failed late callback mutation removed and is back to observe-only behavior at that callback.
- Current source does **not** contain the proposed early `0x815B84E3 + 0x43C` substitution.

Built and deployed DLL:

- Build output: `C:\Destiny 2 Development\Dawn-src\build\x64\Release\steam_api64.dll`
- Live DLL: `C:\Destiny 2 Development\bin\x64\steam_api64.dll`
- DLL timestamp: `2026-08-22 01:06:15`
- Both DLLs have SHA-256:
  `A13A524D8407EE69BB7E0EDE60799576893A7527DEA58B08F2288814A5C01F1E`
- Those two DLLs are byte-identical.
- This deployed DLL still contains the tested late callback Actor-1-to-Actor-2 mutation. The latest log proves it with `omega_scene_vfx_binding actor2_bind/actor2_restore` events.

Before any new test, the new chat must decide which state to build:

1. Build the current observe-only source to establish a clean baseline; or
2. Finish the narrowly scoped earlier source-writer experiment, review it, then build it.

Do not launch another run assuming the current source is already in the live DLL.

## User-visible target and current defect

### Retail target

During the Omega opening:

1. Ikora begins in the authored pre-scene pose.
2. The authored scene plays with animation and dialogue.
3. Purple Void effects originate from the animated Ikora's hands/body.
4. Ikora transitions to the final portal-holding pose.
5. The portal/beam presentation remains correctly attached and visible after the scene for as long as the authored state requires it.

### Current behavior

Across the recent runs, the exact count and lifetime of duplicates changed as lifecycle experiments changed, but the stable core defect remained:

- One Ikora representation animates correctly.
- Another Ikora representation is static, frozen, or T-pose-like.
- The purple outline/aura and beams appear around or from the static representation, frequently near its left hand/body, instead of visually following the intended animated/final actor.
- The VFX is temporally synchronized with the scene; it is not a random unrelated ambient effect.
- At scene completion, the static representation and beam/aura have often disappeared together.
- The persistent final portal-holding visual is still not correct.
- Walking through the expected beam/portal area has caused the screen to wash white, which suggests that at least some non-rendering trigger/volume or effect state is active even when the visible beam presentation is wrong.

Important ambiguity: visual inspection strongly associates the aura/beam with the static actor, but the low-level renderer owner has not been proven to be the Actor-1 object handle. The type-23 created owner is an opaque handle (`0x77F9ED25` in the latest run), and no exact actor pointer match was found for it. Do not convert the visual association into a claimed pointer-level parent relationship without new evidence.

## Confirmed prerequisites that are no longer the blocker

These systems were necessary to reach the scene, but they are not the immediate purple-VFX alignment blocker:

- `mission_scot` launches into the intended Omega/Lighthouse area.
- Authored route and manager activation are far enough along to enter the activity world.
- Private activity membership/host readiness is far enough along for the authored opening content to be published.
- The `D00142CF` object group is published and Ikora is instantiated.
- The Ikora entity definition `0x80EC0F27` is available.
- The authored opening scene can be selected and started.
- The corrected scene-selector body/framing is sufficient for the real scene, animation, and at least Ikora's voice line to execute.
- The two purple scene events are dispatched by the client.

Do not restart this investigation at Steam emulation, Demonware/BAP sign-in, authored route selection, `identity_enable`, manager mode progression, general activity-host connection, basic roster publication, or the original Ikora spawn deficit unless a new run shows one of those prerequisites regressed.

## Relevant authored identities and code sites

### Content identities

- Activity: `mission_scot`
- Bubble: `15` (`0xA83A9175`, Lighthouse)
- Opening slice used during this work: slice set `120`, state `0`
- Complete Ikora/gate registry object: `0x80F47BB2`
- Registry/auth group key: `0xD00142CF`
- Ikora squad/spawner: `0x80F47B70`
- Ikora scene object: `0x80F47B73`
- Gate controller: `0x80F47BA0`
- Ikora entity definition: `0x80EC0F27`
- Ikora presentation component: `0x80EC13A2`
- Opening scene that owns the two purple events: `0x80FCCE87`
- Separate portal-related scene seen elsewhere: `0x80C51CBE`
- Purple effect references: `0x80B9FDBE` and `0x80C220EB`
- Transform-bearing/binding candidate component: `0x815B84E3`
- Three one-member Ikora scene handles observed by the native scene scheduler:
  - `0x80EC0F0E`
  - `0x80EC0FA8`
  - `0x80EC0FA6`

### Pinned runtime RVAs in `activity_spawner_chain_probe.cpp`

These are runtime RVAs for the pinned client image used by the hook code. Do not mix them with Ghidra addresses without accounting for the project's base offset.

- Entity factory: `+0x56D9B0`
- Scene actor scheduler: `+0x5902C0`
- Common scene visual object creation: `+0x575690`
- Visual entry creation (`0x8080992F` path): `+0x9EFBC0`
- Visual transform apply: `+0x5012E0`
- Visual attachment bind: `+0x3F8DE0`
- Authored scene-event dispatcher: `+0x5873F0`
- Type-23 scene callback: `+0x11EE970`
- Type-23 transform resolver: `+0x58A150`
- Transform-bank source-row writer: `+0x58EB40`
- Presentation renderer wrappers:
  - `+0x444A00`
  - `+0x444A80`
  - `+0x4453F0`
  - `+0x445430`
  - `+0x445470`
- Source-kind-2's deeper resolver has been referred to in the investigation as `+0xA1F360`; verify this address and call relationship in the current pinned image before hooking or changing it.

## Actor terminology and what is actually known

The labels Actor 1, Actor 2, and Actor 3 are factory-sequence labels assigned by the recorder. They are not authoritative retail names.

### Factory Actor 1

- Scene handle: `0x80EC0F0E`
- Latest-run object handle: `0x50FAA3C3`
- Created at `t=118391`
- Created from the common descriptor at position approximately `347.397, 249.845, 99.459`
- Recorder heuristic label: `standin_or_replacement`
- No behavior controller was observed in the latest actor snapshot.
- Released around the time factory Actor 3 was created, approximately 16 seconds later.
- The opening-scene context field at `context_data + 0x2C` contained this handle for both purple events.
- The transform-bearing `0x815B84E3` component captured under factory sequence 2 contained this handle at offset `+0x43C`.

Visual inference: this is the leading candidate for the unwanted static/T-pose representation. It has not been proven by an exact renderer-handle identity match, so retain “candidate” status.

### Factory Actor 2

- Scene handle: `0x80EC0FA8`
- Latest-run object handle: `0x71FAA007`
- Created in the same frame as Actor 1 at `t=118391`
- Created from the same authored root placement.
- Has a behavior controller (`0x...5304E120` in the latest run) and recorded behavior updates.
- Remained live while the two purple events fired.
- Released at `t=138578`, after the main scene interval.

Visual inference: this is the best-supported candidate for the animated scene performer. The fact that the transform provenance match is in a component captured under factory sequence 2 strengthens that association, but the component's `object` field is not the same thing as the Actor-2 object handle and must not be conflated with it.

### Factory Actor 3

- Scene handle: `0x80EC0FA6`
- Latest-run object handle: `0x51FAA3C3`
- Created at `t=134391`, about 16 seconds after Actors 1 and 2.
- Created from the same authored placement.
- Remained current/live after Actors 1 and 2 were released in the latest run.

Visual inference: this is the leading candidate for the final/persistent scene replacement or final pose actor. Its exact retail role remains an inference.

### Why root position does not identify the correct actor

Actors 1 and 2 reported the same root position during the purple events. The selected effect transform was roughly 2.8–3.0 units from both roots. That distance cannot say whether the transform came from Actor 1 or Actor 2. The decisive evidence must be a bone/socket transform, source-object handle, renderer attachment, or exact upstream resolver input—not root distance.

Object-table slots are also reused after release. Compare complete handles/generations and timestamps, not only object-record addresses or low index bits.

## Latest-run timeline and hard evidence

Latest log:

`C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log`

The line numbers below refer to the current copy of that file and will become stale if the log is replaced.

### 1. Actors 1 and 2 are intentionally scheduled scene cast members

At log lines around `8666–8836`:

- Scene scheduler `0x80EC0F0E` resolves a one-member slot and creates Actor 1.
- Scene scheduler `0x80EC0FA8` resolves a separate one-member slot and creates Actor 2.
- Both use Ikora entity definition `0x80EC0F27`.
- Both are native scene-scheduler creations, not duplicate requests from the squad spawner recorder.

Consequence: blindly deleting one at factory creation is unsafe. The scene intentionally requested both cast members, even if one is currently presented incorrectly.

### 2. Actor 3 is created later as another intentional scene cast member

At lines around `21549–21666`, `t=134391`:

- Scene scheduler `0x80EC0FA6` resolves its one-member slot.
- It creates factory Actor 3 with definition `0x80EC0F27`.

Consequence: the three actors are phase/cast participants, not three identical accidental spawner emissions.

### 3. The first purple event executes

At lines around `18770–18790`, `t=131172`:

- Event index: `13`
- Scene: `0x80FCCE87`
- Effect reference: `0x80B9FDBE`
- Authored selector: `1`
- Type-23 transform selector observed by the nested resolver: `1`
- Context object before mutation: Actor 1 (`0x50FAA3C3`)
- Actor 2 was current/live (`0x71FAA007`)
- Created effect object: `0x08FCE008`
- Created owner: `0x77F9ED25` (no exact Actor-1/2/3 match)

### 4. The second purple event executes

At lines around `19102–19122`, `t=131359`:

- Event index: `6`
- Scene: `0x80FCCE87`
- Effect reference: `0x80C220EB`
- Authored selector: `1`
- Context object before mutation: Actor 1 (`0x50FAA3C3`)
- Actor 2 was current/live (`0x71FAA007`)
- Created effect object: `0x66FCE00A`
- Created owner: the same opaque `0x77F9ED25`

### 5. The tested late Actor-1-to-Actor-2 mutation did run

For both events the deployed DLL logged:

- `stage=actor2_bind`
- `before=50FAA3C3`
- `actor2=71FAA007`
- `actor2_current=yes`
- `applied=yes`
- `selector_preserved=1`
- followed by `stage=actor2_restore` with `restored=yes`

Inside the nested resolver, `context_object=71FAA007`, proving the temporary write was visible during native resolution.

The scene still used:

- `route=direct_bank`
- `selector=1`
- `object_argument_consumed=no`

The visuals did not move to Actor 2. This result rules out `context_data + 0x2C` as a sufficient late-stage fix for this direct-bank path.

### 6. Bank entry 1 is real and dynamic

The transform-bank writer started at approximately `t=130719`, before the two purple events.

For scene `0x80FCCE87`, it repeatedly logged:

- `kind=2`
- `output_start=1`
- `output_count=1`
- `flags=3C`
- `cast_index=-1`
- `source_index=1`
- `changed=yes`

Its transform changed frame by frame. For example, around the first purple event, selected bank entry 1 had a position near:

`347.821, 250.264, 102.230`

and continued moving during the scene. Therefore this is not simply a constant zero transform or a bank that never updates.

The source runtime row was consistently logged as:

- qword 0: `0x808096631BF9ECF0`
- qword 1: `0x30`
- qword 2: `0x7`

The row did not expose a direct authored cast slot (`cast_index=-1`). That is why the investigation moved below the callback into the source-kind-2 resolver.

### 7. Exact transform provenance points into the factory-2 component graph

For both purple effects, the exact selected 32-byte transform matched exactly once in the captured component corpus:

- factory sequence: `2`
- component definition: `0x815B84E3`
- component address in latest run: `0x...530566A0`
- component size: `0x738`
- exact transform offset: `0x210` (`528` decimal)
- position subfield offset: `0x220` (`544` decimal)

The same component also contained Actor 1's full handle at `+0x43C`.

This is the strongest current evidence, but it does not yet prove what `+0x43C` semantically means. It may be the binding/source handle, a cached context handle, or an unrelated reference in the same component. The proposed earlier A/B test is meant to distinguish those cases.

### 8. Renderer/owner correlation remained unresolved

For both effects:

- Type-23 `context_owner` and created owner were `0x77F9ED25`.
- Exact component-graph scans found no direct Actor-1/2/3 match for that owner.
- Presentation fingerprint scans covered the `0x80EC13A2` runtime graphs for Actors 1 and 2, but no exact pointer equality tied the created owner to either actor.
- `context_object` was Actor 1 and matched many Actor-1 component references plus one reference in Actor-2's `0x815B84E3 + 0x43C` field.

Therefore “the beam renderer is parented directly to Actor 1” is not proven. The confirmed statement is narrower: the authored callback context names Actor 1, while the transform comes from direct bank entry 1 and exact transform bytes occur in the factory-2 component graph.

## What was implemented during the VFX investigation

The main implementation is the large probe in:

`C:\Destiny 2 Development\Dawn-src\Dawn\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp`

It currently contains or has contained the following classes of instrumentation/experiments.

### Scene actor and lifecycle recording

- Hooked the native scene actor scheduler at `+0x5902C0`.
- Captured one-member cast slots, authored records, scene handles, and factory order.
- Hooked the Ikora entity factory and object finalization.
- Recorded object handles, generations, descriptors, root positions, components, behavior controllers, and releases.
- Expanded component capacity so all three Ikora graphs could be retained instead of silently dropping the third actor.
- Added release/deferred-completion observation around the scene handoff.

Useful result: Actors 1/2/3 are separate intentional scene cast creations. They should not be treated as three squad-spawner duplicates.

### Presentation and renderer-handle correlation

- Hooked presentation component `0x80EC13A2` startup and tracked its nested graph.
- Hooked five narrow renderer wrappers at `+0x444A00`, `+0x444A80`, `+0x4453F0`, `+0x445430`, and `+0x445470`.
- Compared renderer-wrapper pointers/handles against all three actor graphs.
- Scanned component graphs for exact object-handle and pointer equality.

Useful result: no exact match tied opaque created owner `0x77F9ED25` directly to Actor 1, Actor 2, or Actor 3. This avoided falsely naming the renderer owner, but it did not fix the visuals.

### Authored scene-event recording

- Hooked the leaf authored event dispatcher at `+0x5873F0`.
- Recorded event type, callback, authored record, runtime record, scene, selector, effect reference, actor set, created object, and owner chain.
- Isolated the exact two purple effect references in scene `0x80FCCE87`.

Useful result: the purple effects are real authored type-23 events, not generic post-scene portal objects or an unrelated squad effect.

### Type-23 callback and transform-resolver recording

- Hooked type-23 callback `+0x11EE970`.
- Hooked transform resolver `+0x58A150`.
- Captured callback context, context owner/object, authored selector/reference, output transform, bank layout, actor fingerprints, and owner candidates.

Useful result: both purple events select bank entry `1` using the `direct_bank` route. The object argument is not used by that route.

### Transform-bank map and provenance recording

- Decoded the bank layout under the opening scene context.
- Recorded all six transforms and metadata entries.
- Compared the selected output against all captured Ikora component bytes.
- Hooked the source-row writer at `+0x58EB40` to capture the row that updates entry 1 before/after each native write.

Useful result: entry 1 is dynamically written by source kind 2, and its exact bytes live in factory-2 component `0x815B84E3 + 0x210`.

### Low-level visual object/transform/attachment recording

- Hooked common scene visual object creation at `+0x575690`.
- Hooked `0x8080992F` entry creation at `+0x9EFBC0`.
- Hooked candidate transform apply `+0x5012E0` and attachment bind `+0x3F8DE0`.

The post-scene objects captured through this path were three `0x80EC0F94` objects created at the authored placement around `347.599, 249.772, 102.482`. In those calls, the expected transform-apply and attachment-bind hooks did not fire. Those objects appear to be the separately published post-scene portal visual trio, not a proven low-level representation of the two dynamic type-23 purple effects. Do not use this result to claim that the dynamic beam has no attachment; it mainly ruled out the initially assumed creation path.

### Server-side post-scene visual publication

The server emitted three portal-visual records for `D00142CF` slots `4/2`, `4/3`, and `4/4`, each with auth and sense bodies. The latest run logged `body_bits=253` and successful 256-bit remainders.

This publication created post-scene objects, but it did not correct the opening scene's misbound aura/beam, did not make the visible beam persist correctly, and did not make the portal entrance functional. It is a separate state/persistence layer and should not be used as the next lever for the immediate opening VFX alignment bug.

### Temporary late callback mutation

The deployed DLL temporarily replaced `context_data + 0x2C` from current Actor 1 to current Actor 2 only for references `0x80B9FDBE` and `0x80C220EB`, preserved selector 1, called the original function, and restored the original handle immediately.

It was guarded by scene/effect/current-handle checks and logged successful apply/restore for both events.

Result: no visual correction. The nested resolver saw Actor 2, but `route=direct_bank` and `object_argument_consumed=no` proved the change was too late and irrelevant to transform selection.

Current source has this mutation removed. The deployed DLL still has it.

## Attempts and wrong turns, with their actual outcomes

The exact source of every intermediate DLL is not preserved because the main probe file is untracked. This section records the reliable behavioral outcome of each approach rather than pretending every old build can be reconstructed.

### Correcting scene selector/body framing

Intent: make the native scene start with a real nonzero selector instead of malformed/neutral state.

Outcome: successful. Ikora's actual authored animation and voice line played. This was a prerequisite fix, not the purple binding fix.

Status: keep the working scene activation path. Do not regress it while debugging VFX.

### Treating all visible Ikoras as accidental spawner duplicates

Intent: suppress extra entity creation so only one Ikora remained.

Observed outcomes:

- Some transient duplicate appearances were reduced.
- One build removed a brief extra Ikora during the Void/scene action.
- Aggressive suppression also removed or damaged the beginning of the cutscene.
- An Ikora could still appear later because the native scene scheduler intentionally creates another cast member.

Conclusion: the three factory sequences are not merely repeated squad-spawner requests. Actor lifetime/presentation may still be wrong, but deleting scene actors at factory creation is not a sound VFX fix.

### Hiding or retiring the candidate T-pose actor by lifecycle timing

Intent: remove Actor 1 when Actor 2 or Actor 3 became available.

Observed outcomes:

- The visible actor count changed across builds.
- At one point only two representations remained instead of three.
- In other runs the scene opening was damaged or the representation returned later.
- The purple visual still originated from the wrong place.

Conclusion: actor lifecycle and VFX source binding are related in presentation but are not solved by a blind release/hide. The scene can recreate or transition cast members, and the VFX may consume transform state independently of renderer visibility.

### Forcing the final pose or post-scene handoff

Intent: retain the final portal-holding Ikora/beam after the scene.

Outcome: final-state objects and handoff activity were observed, but the opening beam/aura remained misaligned and visible persistence remained wrong.

Conclusion: persistence is a later phase. It cannot fix a transform that is already wrong during the scene.

### Manually moving/placing portal objects

Intent: make portal visuals appear at the authored world placement.

Outcome: objects could be created at the expected authored root placement, but dynamic beam alignment, actor attachment, and portal behavior remained wrong.

Conclusion: a world-space placement is not equivalent to an animated hand/socket binding.

### Renderer wrapper suppression/correlation

Intent: identify and hide the static renderer or reassign the VFX renderer.

Outcome: no definitive renderer-owner match was found, and visual suppression attempts did not reliably remove only the unwanted representation.

Conclusion: the wrappers captured presentation activity but did not establish the beam's source transform. Continue only if a future source trace identifies a specific wrapper/handle.

### Treating selector “2” as Actor 2

Intent: bind the effect to the second actor by changing a selector/index to 2.

Outcome: this model was wrong. Selector numbers are transform-bank indices, not factory actor numbers. The two purple events author selector 1, selected bank entry 1 exactly matches the native output, and entry 1 is dynamically updated.

Conclusion: preserve selector 1 unless static data proves the authored event itself is malformed. Do not equate Actor 2 with selector 2.

### Rewriting callback context object to Actor 2

Intent: make native VFX resolution use Actor 2.

Outcome: mutation definitely applied and restored; nested resolver saw Actor 2; visuals did not change.

Conclusion: ruled out as a sufficient fix. The direct-bank resolver ignored that object argument.

### Chasing the generic `0x8080992F` portal-visual path as the live beam

Intent: capture the final transform and attachment through entry-create/transform-apply/attachment-bind hooks.

Outcome: captured separately published `0x80EC0F94` objects, but not a complete dynamic type-23 beam attachment chain.

Conclusion: useful for final portal persistence later, but it is not yet proven to be the opening scene beam path.

## Things now ruled out or strongly deprioritized

### Ruled out as the immediate cause

- “The purple events never fire.” They fire.
- “The effect references are absent.” Both references are present and create objects.
- “The transform selector is zero or missing.” It is selector 1.
- “The selected transform never updates.” Entry 1 changes frame by frame.
- “Changing the callback's object field to Actor 2 will make direct-bank resolution follow Actor 2.” It did not.
- “Selector 2 means Actor 2.” It does not.
- “All Ikora instances are accidental squad-spawner duplicates.” The native scene scheduler intentionally creates three one-member casts.
- “The three post-scene `0x80EC0F94` objects alone explain the dynamic opening beam.” They are a separate path.

### Strongly deprioritized for this focused visual task

- General matchmaking/session repair.
- Steam callbacks or SteamID emulation.
- Manager `identity_enable`/`mgr_activate` work.
- Generic membership region or roster transport, unless those regress.
- Objective progression.
- Gate collision/travel.
- Ghost dialogue.
- Enemy/AI authority.
- Mission-script progression after the opening.
- The index-heap “double-free?” assertion seen when closing the game with the window X. It should be fixed eventually, but the user reports it corresponds to shutdown rather than the in-scene visual defect.

## What remains possible

The following are still live hypotheses. They are ordered by current evidence, not certainty.

### Hypothesis A: `0x815B84E3 + 0x43C` is the upstream object/source binding

Evidence for:

- The exact live selector-1 transform is in this component at `+0x210`.
- The same component contains the current Actor-1 handle at `+0x43C`.
- The component is captured under factory sequence 2, which is associated with the animated scene performer.
- The late callback object swap was too late; this field exists at the earlier component/source layer.

Evidence missing:

- No direct proof yet that source-kind 2 reads `+0x43C` while producing bank entry 1.
- No proof yet that changing it changes the 32-byte bank transform.
- No proof yet that it is a binding field rather than a cached or unrelated reference.

### Hypothesis B: source-kind-2's deeper resolver selects a cached/static marker or stand-in

The source row has no cast slot and appears to resolve through an internal runtime identity. The deeper resolver referred to as `+0xA1F360` may choose a marker, scene object, or cached transform independent of `context_data + 0x2C`.

If so, the correct fix is in that resolver's input/source binding, not the callback and not a manual world offset.

### Hypothesis C: bank entry 1 is an animated transform, but it is the wrong socket/marker

Entry 1 moves frame by frame and sits roughly 2.8–3.0 units above the shared actor root. That is compatible with a hand/socket transform, but it may be the hand/socket of the wrong cast representation or a static authored helper that animates in scene space.

The next trace must compare it to actual Actor-2 animated bone/socket transforms, not only Actor-2's root.

### Hypothesis D: the effect copies/caches the transform before the attempted mutation

If the relevant transform is copied into the bank or effect runtime earlier than the type-23 callback, any callback-time object mutation will be ineffective. The source writer beginning before the events supports this timing possibility.

### Hypothesis E: duplicate actor presentation and beam binding are two separate bugs

It is possible to fix the beam source and still retain an unwanted static renderer. It is also possible to hide the renderer while leaving the beam wrong. Success criteria must check both independently.

### Hypothesis F: missing/neutral authoritative attachment state causes a client fallback

The server may still be supplying a neutral binding or attachment field that lets the scene run but makes the client choose a fallback source. Current evidence points into native source resolution, but an upstream authored-state omission is not fully excluded until the source row's identity and writer are decoded.

## Recommended next investigation

Do not make another broad actor-lifetime, portal-state, or renderer-hiding change. Keep the experiment limited to scene `0x80FCCE87`, the two purple references, and bank entry 1.

### Step 0: preserve evidence and reconcile the binary

1. Copy or archive the current `dawn.log`; it contains the successful late mutation and source-provenance evidence.
2. Confirm the live DLL hash before testing.
3. Review current source because it is untracked.
4. Build/deploy only after deciding whether the next DLL is an observe-only baseline or the early source experiment.

### Step 1: verify the source-kind-2 call chain statically

Before another game run:

1. Re-open the decompile around `+0x58EB40` and its caller.
2. Confirm where source kind 2 dispatches.
3. Confirm the current pinned address and signature of the deeper resolver currently called `+0xA1F360`.
4. Identify whether the resolver receives:
   - the `0x815B84E3` component,
   - the source runtime row (`808096631BF9ECF0, 0x30, 7`),
   - an object handle,
   - a transform/provider pointer,
   - or a static marker identity.

This static audit should happen before adding another mutation.

### Step 2: add one focused resolver/source recorder

The recorder should log only when all of these are true:

- forced activity is `mission_scot`;
- scene is `0x80FCCE87`;
- source kind is `2`;
- output start/count selects entry `1` only;
- source index is `1`;
- Actor 1 and Actor 2 are both current and live.

Capture:

- source runtime row and all pointers passed into the deeper resolver;
- component identity and address;
- `0x815B84E3 + 0x43C` before/after;
- the exact 32-byte bank entry before/after;
- any returned object/provider/transform pointer;
- exact full Actor-1 and Actor-2 handles;
- if available, the animated socket/bone transform used by Actor 2.

Do not log every frame without a sample cap/change filter.

### Step 3: perform the controlled `+0x43C` A/B only if Step 1 supports it

If the static/runtime call chain shows that `+0x43C` is read as the source handle:

1. Locate the `0x815B84E3` component captured under factory sequence 2.
2. Require size `>= 0x440`.
3. Require field `+0x43C` to equal the current full Actor-1 handle.
4. Require Actor 2's full handle to still be current in the object table.
5. Require the exact Omega scene/source row filters above.
6. Save the original value.
7. Temporarily write current Actor 2 only around the original source-writer/resolver call.
8. Immediately restore the original value if it is still Actor 2.
9. Log apply and restore success.
10. Compare the exact 32-byte bank entry before and after the native call.

This is an A/B test, not a claimed final fix.

### Step 4: branch on an objective result

#### Outcome A: field swap changes bank entry 1 and the VFX moves correctly

Then `+0x43C` participates in source binding. Replace the temporary hook with the correct durable authored/runtime binding at component construction or authoritative-state application. Preserve selector 1 and native animation timing.

#### Outcome B: field swap does not change bank entry 1

Then `+0x43C` is ruled out as the active source for this writer. Remove the mutation. Follow the actual pointer/provider returned by the deeper source-kind-2 resolver. Do not repeat callback or renderer-owner swaps.

#### Outcome C: bank entry changes, but the VFX does not

Then the effect is caching/copying another transform or the wrong bank consumer has been identified. Trace the exact copy from bank entry 1 into the type-23 created object's runtime state and identify when that copy occurs.

#### Outcome D: VFX moves to Actor 2, but the static Ikora remains

Then beam binding and duplicate presentation are separate. Keep the proven VFX fix and start a second, narrowly scoped presentation-lifecycle task for the static renderer.

#### Outcome E: VFX follows Actor 2 during the scene but fails after Actor 3 appears

Then implement the same proven binding model across the Actor-2-to-Actor-3 handoff. Do not guess Actor 3's role before recording the source field change at `t≈134391`.

### Step 5: handle persistence only after in-scene alignment is correct

Once the purple aura/beam originates from the correct animated/final actor during the scene:

1. Trace the Actor-2-to-Actor-3 source/binding handoff.
2. Publish/retain the correct gate-controller and final visual state.
3. Confirm the effect remains visible for the intended authored duration.
4. Only then resume portal entrance/travel work.

## Concrete success criteria

The visual task is complete only when all of these are observed in one run:

- No incorrect static purple/T-pose Ikora is visible on initial arrival.
- The intended pre-scene Ikora presentation is visible.
- The authored animation and voice line still play.
- Purple aura and beams originate from the animated Ikora's intended hands/body throughout the scene.
- The effect follows the animated pose instead of a shared root/static marker.
- The Actor-2-to-Actor-3/final-pose transition does not create an extra visible Ikora.
- The final portal-holding actor and VFX remain in the correct authored state after the animation.
- No scene-start regression, crash, or in-world index-heap corruption is introduced.

## What not to do next

- Do not claim certainty before a visual A/B result.
- Do not equate factory number with transform selector number.
- Do not mutate `context_data + 0x2C` again for this direct-bank path.
- Do not manually offset the beam in world space.
- Do not delete an Ikora at factory creation merely because it looks duplicated.
- Do not use root-position distance to identify the animated actor.
- Do not mix portal persistence work into the in-scene alignment experiment.
- Do not add another broad renderer hook before the upstream source-kind-2 resolver is understood.
- Do not run with source and deployed DLL out of sync.
- Do not trust ordinary `git diff` to preserve the probe; it is currently untracked.

## Useful log queries

Run these from `C:\Destiny 2 Development`.

```powershell
rg -n "ev=omega_scene_vfx_binding" "bin\x64\Dawn\logs\dawn.log"
```

```powershell
rg -n "ev=omega_scene_type23_trace stage=(callback|attachment|owner_candidates)" "bin\x64\Dawn\logs\dawn.log"
```

```powershell
rg -n "ev=omega_scene_transform_source stage=selection|ev=omega_scene_transform_bank_map|ev=omega_scene_transform_provenance" "bin\x64\Dawn\logs\dawn.log"
```

```powershell
rg -n "ev=omega_scene_transform_writer stage=(slot_update|source)" "bin\x64\Dawn\logs\dawn.log"
```

```powershell
rg -n "ev=omega_scene_cast_slot|ev=omega_scene_activation_owner|ev=omega_ikora_followup stage=factory|ev=omega_ikora_ownership stage=(actor_state|object_release)" "bin\x64\Dawn\logs\dawn.log"
```

```powershell
rg -n "ev=omega_beam_final_transform|ev=omega_scene_vfx_trace" "bin\x64\Dawn\logs\dawn.log"
```

## Build and deployment

Build and deploy with:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1"
```

The game must be closed before deployment. After a build, verify the output and live DLL hashes match before launching.

## Relevant local analysis artifacts

Primary source and log:

- `C:\Destiny 2 Development\Dawn-src\Dawn\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp`
- `C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log`

Static analysis outputs:

- `C:\Destiny 2 Development\omega_actor_visual_boundary_out.txt`
- `C:\Destiny 2 Development\omega_rendered_pose_trace_out.txt`
- `C:\Destiny 2 Development\omega_visual_consumer_out.txt`
- `C:\Destiny 2 Development\omega_beam_handlers_out.txt`
- `C:\Destiny 2 Development\omega_scene_scheduler_out.txt`
- `C:\Destiny 2 Development\omega_scene_schema_data_out.txt`
- `C:\Destiny 2 Development\omega_scene_authority_decomp.txt`
- `C:\Destiny 2 Development\omega_object_lifecycle_out.txt`
- `C:\Destiny 2 Development\omega_post_scene_consumers_out.txt`
- `C:\Destiny 2 Development\omega_visual_auth_schema.txt`

Ghidra scripts created during this investigation:

- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaActorVisualBoundary.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaRenderedPose.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaVisualConsumer.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaSceneScheduler.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaSceneSchema.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaSceneSchemaData.java`
- `C:\Destiny 2 Development\ghidra_scripts\DecompOmegaBeamHandlers.java`
- `C:\Destiny 2 Development\ghidra_scripts\DecompOmegaSceneAuthority.java`
- `C:\Destiny 2 Development\ghidra_scripts\DecompOmegaObjectRetirement.java`
- `C:\Destiny 2 Development\ghidra_scripts\TraceOmegaPostSceneConsumers.java`

## Short prompt for the next chat

Use the following when starting the next task:

> Read `C:\Destiny 2 Development\OMEGA-PURPLE-VFX-HANDOFF-20260822.md` completely, then inspect the current untracked `activity_spawner_chain_probe.cpp` and the latest `dawn.log`. Do not modify or build anything until you reconcile the source/DLL mismatch documented in the handoff. Focus only on the Omega opening purple aura/beam alignment. The tested late `context_data+0x2C` Actor-1-to-Actor-2 swap is ruled out because `+58A150` used `route=direct_bank` and `object_argument_consumed=no`. First verify the source-kind-2 path from `+58EB40` into the deeper resolver currently called `+A1F360`, and determine whether factory-2 component `0x815B84E3 + 0x43C` is actually read as the source handle for bank entry 1. Only then implement the guarded temporary source-writer A/B described in the handoff. Label every claim as confirmed, inferred, or untested; do not promise a fix before the visual result.

## Bottom line

This is not back at square one, but it is also not fixed.

The investigation has moved from “purple visuals are wrong” to a bounded chain:

`scene 80FCCE87` → `type-23 events 80B9FDBE / 80C220EB` → `selector 1` → `direct transform bank` → `entry 1` → `source kind 2 / source index 1` → exact transform in factory-2 `815B84E3 + 0x210` → unresolved source/binding candidate at `+0x43C` or beneath the source-kind-2 resolver.

The callback object field, selector renumbering, generic renderer ownership, blind actor deletion, and generic portal-object placement have all failed to solve this specific alignment bug. The next useful result must come from proving or disproving the upstream source binding that generates bank entry 1.
