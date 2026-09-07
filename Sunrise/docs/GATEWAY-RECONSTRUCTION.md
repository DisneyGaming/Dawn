# The Gateway reconstruction

Current shared-service integration: [Universal mission services](UNIVERSAL-MISSION-SERVICES.md). Gateway now exercises readiness, object/destructible lifecycle, scene milestones, event-relative timing, marker lifecycle, stall diagnostics and shared completion. The older acceptance records below remain historical; this integration requires a fresh in-game run.

Current integration: `gateway.ending.v2`, format-2 mission JSON. The complete ending was validated live, including the shielded cube and portal blocker, native Vance turn and conversation, timed Lighthouse ascent, and native activity phase 6 / result 1. This integration replaces those temporary memory overrides with retained mission authority. A fresh run of the assembled DLL remains the final integration check.

The JSON keeps the existing opening, combat cohorts, wave order, boss/module destruction gates, and dialogue bindings. Its ending now waits for the actual native conversation to start, requests both Lighthouse controls at 22,640 ms (the validated “lost prophecies” cue), and finishes at 31,000 ms after the ascent requests. The existing lifetime writer publishes phase 6 / result 1 and retires the objective. It does not request another activity.

The native adapter supplies the following bindings:

- Module source `4B946B28/4/32`, entrance blocker `/4/27`, and beam `/4/33`: publish inactive preparation, then a fresh generation for creation. All three are present from arrival. Module position 1 is visible and shielded. Final boss clearance changes it to .75, removing its shield while retaining the same health identity. Real cube destruction retires the module and beam and opens the entrance blocker. Boss death alone cannot open the portal.
- Vance source `BA0B27A0/1/4` and scene `/43/5`: create the cast and idle scene from arrival. The accepted dispatch of entrance invitation row 10 releases event `3A5C256C`, so his invitation accompanies his native turn. Ghost row 9 follows the invitation. Actual turn completion, a retained player approach, and completion of both entry lines release event `C2656F80` for his native conversation. Rows 11–15 remain scene owned.
- Scene observation: re-resolve the salted selector weak reference after the native tick; qualify root, turn, hold and child identities/states. Only the live `80EC0AC5` child starts the cue clock. Stale run/generation/group/sensor/selector receipts and duplicate starts cannot advance or reset it. The native actor idle remains active after the spoken ending, so native scene completion is recorded separately and is not fabricated by the cue clock.
- Lighthouse walls `BA0B27A0/23/0` (`80F46DD0`) and lighting `/23/1` (`80F46DD3`): position 1 with snap closes/dims the room from arrival. Position 0 without snap performs the native ascent/lighting change at the cue. Device revisions advance across controller restarts.

The exit teleporter and earlier objective-marker issue remain deferred. The reverted stationed-squad and actor-limit experiments are not included. Gateway checkpoint recovery still requires its own live validation.

Validation entry point: `python tools/coo/verify_gateway_ending_integration.py`. It checks package/live-capture identities, protects unrelated accepted sources, runs Debug/Release Gateway and protocol/scene regressions, builds the DLL, and freezes the candidate sources and script. Live evidence is under `build/coo/gateway-vance-live-20260907` and `build/coo/gateway-module-invisible-20260907-023330`. Useful receipts: `module_bound`, `module_destroyed`, `vance_scene_started`, `vance_turned`, `vance_conversation_started`, and `mission_finished=1`.

The sections below retain the investigation history; their candidate-stage limitations describe those earlier builds.

## Launch the preload

After the candidate is installed, restart through the usual launcher. In **Activity override**, click **Gateway opening**, then launch **Chosen** from the Director. The preset enables the override and selects:

- Activity `mission_abs`, native investment index **292**.
- Bubble **15**, `lighthouse` (`A83A9175`).
- Slice set **120**: bubble 15, state 0, packed with the native factor of eight.
- Spawn set **0x69F52B3E**, selected automatically by the updated preset. Package analysis places its three points inside the opening landing-zone volume, and the user confirmed this is the correct in-game arrival.

The selection stays staged until Chosen supplies the exact 282/282 native launch opportunity. The shared prelaunch hooks change it to 292/292/mission_abs before the native state-0 producer derives and publishes the contract. Gateway's server route requires this native-derived tuple and a usable captured descriptor; it does not accept a late Chosen rename. Clear disables the process-local selection. Towerfall retains its existing profile and legacy authored fallback.

The preload does not enroll Gateway in Omega's host-authority, roster archive, enemy catalogs, or retry experiments. Geometry arrival alone does not prove mission initialization.

Expected live evidence:

1. `gateway_direct stage=install ... result=ok` (the hooks can already be installed by Towerfall).
2. `gateway_direct stage=selection_contract result=corrected`, source and destination 292, package mission_abs.
3. `gateway_direct stage=prelaunch_publication result=accepted ... native_publish=1`.
4. Service-6 allocation, world entry, bubble 15/region 120, player arrival and Gateway-specific roster contents.

## Native evidence

The installed package tables independently join activity **292**, investment hash **5A2E3FF4**, package definition **986985D0**, activity asset **80F46D99**, scenario **80F46DB0**, and launch descriptor **80F9FDD2**. The scenario has 20 bubbles. Raw offsets and source hashes are recorded in `build/coo/gateway-research/gateway-launch-evidence.json`.

The main gameplay registry is in Lighthouse bubble 15/state 0. The scenario root is **80F4742C / 986985D0**. A different object uses the same registry key in bubble 3/state 1; these variants must not be published interchangeably.

Recovered presentation sources:

- Dialogue: root type 53/index 2, descriptor **80F47426 + 1408**, dialogue bank **80F1FC9E**, 16 selector rows.
- Objectives: root type 68/index 0, descriptor **80F47420 + B88**, directive bank **80F4741F**, seven rows.
- Objective strings: container **80C71C82**, English data **80B9E8FC**.

The current cached authored overlay is empty for Gateway's main slice because its local group count exceeds the five-group cache capacity. The individual groups are cached. The Gateway foundation overlay validates nine pinned groups and every retained descriptor before publication, preserving the original global services and Lighthouse scope. It keeps the group set stable across the opening. Do not widen or substitute Omega's roster as a shortcut.

The two main encounter registries (**4B946B28**, **85742F3E**) and the smaller **BA0B27A0** registry contain 127 type-1 sources in total. Their existence is inventory evidence, not a license to activate all sources at once. Source-to-encounter mapping, category choices, placements and native admission/death receipts remain to be verified. The experimental `gateway-spawn-sources.json` first-entity field is unresolved for multi-choice rows and must not be used as an authority catalog.

## Walkthrough evidence

Reference: [IGN Gateway gameplay](https://www.youtube.com/watch?v=IB3aLj04ZT8), 10:41. The supplied transcript also describes prologue cinematics, which are absent from this gameplay video. The video was sampled at ten-second intervals and selected encounter boundaries; this is not an exhaustive frame-by-frame review.

Observed sequence: Mercury arrival and portal; Vex traversal and man cannons; Forest gate approach and Hydra encounter; blocked Forest portal; return through Descendant Vex; Lighthouse Module Minotaur; vulnerable intelligence module; enter Lighthouse and speak to Vance; mission-complete overlay.

Native objective text explicitly makes the intelligence module vulnerable after the Module Minotaur dies. The video ends at the Lighthouse. It does not establish an automatic launch of the next EDZ mission.

## Validation and next boundary

`python tools/coo/verify_gateway_prelaunch.py` stages a DLL under `build/coo/validation-gateway-prelaunch`, runs launch routing and shared/Omega regressions, and preserves source hashes. It does not install or launch the game.

The native preload now has live confirmation of `292 -> 292 / mission_abs`, Lighthouse slice 120, world entry and a controlled player entity. The first run remained black because arrival/fade completion depended on another spawn-gate call after the player had already spawned. The corrected opening spawn and frame-based fade release were accepted in the live run recorded by `build/coo/validation-gateway-spawn/acceptance.json`. Enemy encounters, presentation progression, retries and completion still need a Gateway profile/document and native validation.

### Black-screen diagnosis and arrival recovery

Read-only inspection of process 59012 found the native world ready (lifetime 3, local readiness true, loader current/queued handles absent) and controlled-player getter `4B2260` returning `53FAA000`. Sunrise's world phase remained `transitioning`, with its arrival/fade-release flag false, despite the native `in_world` entry at log tick 416562. The spawn dispatcher skips its ordinary spawn path once the player entity exists. A timeout or disabled spawn hold can therefore allow spawning before step 38 and strand the later fade release.

The lifecycle weak handle at `+0x50` was initially mistaken for the controlled body. Its absence and a cached teleport `no_body` message do not prove failed character creation; the native controlled-player getter supersedes that hypothesis. A later memory sample was taken after the user had returned toward orbit and is labeled separately in `build/coo/gateway-research/live-arrival-diagnosis.json`.

The shared fix observes world phase on the existing camera frame poll. If a controlled entity is present, native lifetime is 3, local readiness is true and the loader is readable/idle, it completes the pending arrival once and releases the world-transition fade. Spawn permission, coordinates, package identity and mission authority remain governed by their existing paths. The frame sampler participates in the spawn owner's call gate and protected detachment entries.

`python tools/coo/verify_gateway_arrival.py` stages this repair separately and protects every other source/script file from the installed preload archive. Its new source-linked policy tests cover the final spawn call occurring before arrival, deferred release while the loader is busy, missing native evidence and repeated-frame deduplication. These tests do not replace native validation. The expected new receipt is `stage=spawn_arrival result=completed source=frame`, alongside `stage=fade_release result=issued`.

## Opening spawn recovered from package geometry (2026-09-06)

The user confirmed `mission_abs`, bubble **15**, slice **120**, spawn set **69F52B3E** is the correct opening. Bubble/slice chooses the loaded content state; the separate spawn-set hash chooses the player arrival points. The updated Gateway opening preset fills all three fields and highlights the matching spawn row.

The evidence chain is the authored `leadup_lz_start_player_monitor` at `80F473D0 +270`, which references registry `85742F3E`, type 60, slot 359. Its volume record at `80F470E5 +11B0` names `leadup_lz_start_trigger_volume`. This contains a twelve-vertex polygon with vertical bounds -8.5 to -3.5. Scanning 464 native spawn points across Mercury map packages 0356, 03A6, 03A7 and 03A8 found exactly three points inside that volume, all with hash `69F52B3E`:

- (-773.25, -132.00, -7.50)
- (-771.00, -129.50, -7.50)
- (-772.25, -130.75, -7.50)

The spawn records are in tag `80F50039`, class `80809162`, from `w64_mercury_destination_03a8_5.pkg`. The package analysis is spatial authoring evidence; the subsequent in-game validation was confirmed by the user. An explicit native mission-start command has not been recovered. Other sets near x=350 are by the Infinite Forest gate and must not be inferred to be the opening just because they share the Lighthouse bubble.

Reproduce with `python tools/coo/identify_gateway_opening_spawn.py`. The resulting `build/coo/gateway-research/gateway-opening-spawn-evidence.json` includes the reference chain, polygon, matching points and decoded-tag SHA-256 values. The research artifact preserves its original pre-test status. The later user confirmation is recorded here; the preset change is staged under `build/coo/validation-gateway-spawn`. The existing prelaunch routing test verifies that the preset delivers the confirmed hash to the native destination override.

## Opening foundation candidate

`Sunrise/scripts/gateway.json` is an executable format-2 document, with profile `gateway.opening.v1` and the `otherMissions` authority schema. It is read once from the DLL-relative scripts directory on Gateway selection. Invalid/missing scripts fail closed with a `coo_script mission=gateway result=failed` receipt. It does not use the Omega document or the Omega archive encoder.

The native adapter admits the main slice's complete retained groups at initialization after checking registry, object tag, type, slot, flags, descriptor tag/offset, component class and authority/sense schema. It preserves existing global groups and uses cache indices only as hints. This resolves the five-group extraction overlay limit without modifying the shared cache format.

The opening graph waits for the authenticated local player inside the exact landing-zone polygon. It then publishes objective **C8DC7CFD** and requests dialogue bank **80F1FC9E**, row **1**. Row 0 belongs to loading and is excluded from this gameplay queue. The existing native dispatch hook acknowledges only the exact **80F47426 +1408** component, bank, row, run and publication generation. A dispatch timeout does not acknowledge dialogue. The graph's final observation is entering `leadup_recess_encounter_start_trigger_volume`, slot **361**. Its completion means the foundation was checked; it never sends mission completion.

The adapter records the first entry into each of 73 recovered volume polygons after opening arrival. It does not infer enemy deaths, device completion or scene completion from position. Publication generations remain unique across controller resets, including a reused mission run ID. Native Gateway death/checkpoint retry behavior is not yet validated.

The complete video/package mapping is saved in `Sunrise/docs/gateway-reconstruction-map.json`. It contains 15 phases, all 16 dialogue rows, all seven objective events, 127 decoded source definitions, approximate video intervals, and explicit unresolved bindings/counts. This research map is not executable. In particular, Module Minotaur death only opens the next objective: the intelligence module must receive its own destruction receipt. Vance's later lines are conservatively reserved for native Scene ownership pending verification.

Generate/verify the pinned catalog with `python tools/coo/generate_gateway_catalog.py --check`. Build/test with `python tools/coo/verify_gateway_foundation.py`. The candidate needs native validation of full roster seeding, visible opening objective, audible Ikora/Vance opening exchange, and traversal entry receipts. It does not yet recreate the remaining combat or ending.


## Traversal candidate (2026-09-06)

The executable profile is now `gateway.traversal.v1`. Its 21-step graph retains the accepted arrival/objective/Ikora opening, then runs recess, shelf and final-ledge encounters. It finishes only after the local player enters native landing volume 378 at the Lighthouse. It sends no activity-completion or next-mission request.

The JSON selects the order of six population cohorts, reinforcement triggers, clear joins, cannon requests and Vance dialogue row 2. The trusted native catalog pins 67 source identities: 23 killable traversal sources and 44 separate marcher sources. Counts are one actor per authored category (25 required actors total); alternative templates within a category are not extra actors. These counts and wave assignments are reconstruction choices, not recovered retail spawn counts.

The shared PopulationService records full actor and source-owner handles, run and generation. Native A0D510 successful creation proves admission; qualified C72390 health death proves death. Duplicate, stale, foreign and unadmitted deaths cannot advance a clear. Unexpected additional required actors stop progression. Marchers never contribute to a kill barrier. Clearing the initial cohort or entering the reinforcement volume can start the next cohort; each cannon clear still requires both cohorts to die.

Native source authority uses 80807EC9, including canonical absent rules for inline authored placement and the two-category extension for shelf sources 229/230. Sources keep their native templates and placement providers. Marchers bind their authored type-3 tactical row 0. Their type-34 collections use scoped source selectors 80809579, registered at ABB27E to ABA810/502770/5009C0. Each collection selects its four individual marcher sources. Type-26 shield effects reference those collections; they never select players. Selectors and shield holders are published before marcher requests. Marcher phase-block devices use the native position channel; precise retail phase choreography still needs native comparison.

The two authored type-4 cannon cores (slots 3/4, definitions 80F470F1/80F470F4 +4C8) first receive inactive generation state. The existing post-original type-4 apply hook copies and validates that state. Both exact preparation receipts and the recess clear are required before activating either core. Devices 0/1 open with that clear; device 2 opens after the final ledge clears. Static device publication is a request, not a fabricated apply receipt. Final traversal proof is actual landing volume 378. Device polarity, visual timing, AI movement and shield application require the native playthrough; offline tests cannot establish these behaviors.

`python tools/coo/verify_gateway_traversal.py` protects every other source/script against the accepted foundation archive, checks package identities and independently reflected wire fixtures, runs Debug/Release regressions, and builds an immutable candidate under `build/coo/validation-gateway-traversal`. The new codec helpers are shared under `coo`; existing Omega entry points delegate to them with unchanged defaults and are checked against accepted packet fixtures. No additional native detours were added. Existing admission/death and cannon-apply observers route copied evidence to the selected mission.

Tests cover the full synthetic traversal, both reinforcement paths, missing deaths, delayed cannon preparation, per-row dialogue receipts, unchanged roster membership, full wire encoding, independent native source/shield/collection bytes, population overflow, and stale callbacks after a reused-run restart. This does not establish Gateway checkpoint/wipe recovery in game. The accepted foundation, including its script, remains in the immutable foundation archive for rollback.


## Traversal position-recovery fix

The first traversal run loaded the correct DLL and format-2 document, admitted all 12 live roster groups, dispatched opening row 1, and received both native inactive cannon-core preparation receipts. It then stayed at executor active mask 4 / complete mask 3, waiting for recess volume 361. No traversal population or phase-block request had been issued.

Read-only inspection of process 60980 found the published player position frozen at (-766.047, -133.767, -6.695), inside the arrival area. The cached physics pointer had been retired: its full owner field was 0 and its body index was -1, while native controlled getter 4B2260 returned 08FAA000. The old ownership predicate compared only the low 13 pool-index bits, so it considered these different handles equal and kept the stale cache. Evidence is preserved in `build/coo/gateway-traversal-entry-failure/diagnosis.json` and `live-position.json` in the same directory.

The shared tracker now verifies full salted ownership around the body read, rejects non-finite coordinates and evicts unreadable/retired components. The next owned physics sample reacquires the player. A test compiles production `player_position.cpp`, reproduces the exact zero-owner/same-index failure, and drives the unchanged Gateway graph through recess entry after reacquisition. Additional cases cover unreadable bodies, fallback pointers, a handle changing during a read, and one publication per successful poll. The source guard also checks that the native ownership function delegates to the full-handle reader.

The Gateway JSON, wave counts, devices and codecs are unchanged by this repair. Native validation of the newly reachable traversal remains pending. Build with `python tools/coo/verify_gateway_position.py`; it preserves the installed traversal archive and stages a separate candidate.


## Mainland continuation and traversal route correction

The next playthrough validated position recovery and native traversal admission/death receipts. It stalled at the shelf clear (active mask 0x2000): source 231 / actor 75F42037 had an admission but no death receipt. The player reached the final ledge, flew to the mainland, and approached the Forest entrance while the graph still waited on that shelf actor. No final-ledge source had been requested. The saved run is `build/coo/gateway-final-cannon-research/sunrise.log`.

Profile `gateway.mainland.v1` now starts the 44 marcher sources and their phase-block devices after verified world arrival in the opening landing polygon, independently of opening dialogue dispatch. The first required encounter still starts at recess entry. The shelf transition accepts either both shelf cohorts cleared or physical entry into the final-ledge commit volume. Final-ledge entry also releases the preceding shelf reinforcement trigger if its smaller volume was missed. This is a route observation, not an enemy-death receipt: a surviving shelf actor remains tracked, and its later native death can still be accepted. Final-cannon activation still requires real admission and death of both final-ledge cohorts. Its existing type-23 channel publication is unchanged.

The mainland landing now requests 19 authored sources in registry 4B946B28: 12 actors around the outskirts and 15 central defenders, including the authored anchor/Hydra source 49. Counts remain one request per authored category; exact retail counts are not established. Outskirts actors are a route encounter, while the central cohort must receive all qualified native deaths before the Forest approach proceeds. Source lookup now requires registry plus slot because mainland slots overlap traversal marcher slots. Only cohort zero receives marching tactical orders. Logs include the registry for each admission/death.

The graph publishes the mainland objective and Vance's gateway description (row 3), then waits for central clearance and the stairs approach before Ghost's row 4. Entering the blocked Forest entrance requests row 5 and then publishes Bring Sagira to Brother Vance. Rows use the existing serialized dialogue service and exact native dispatch acknowledgements. Mainland gate devices retain their native state; this scope does not request a Forest transition. The agreed endpoint is blocked-entry dialogue, with the return fight, Lighthouse finale and mission completion deferred.

The saved reference video was reviewed again using the 2:00-3:50 and 4:00-5:50 contact sheets and the 5:38 still. The final ledge has its own Minotaur encounter before the launcher flight; the later central fight and blocked entrance are separate phases. Exact marcher/platform choreography and visible final-cannon activation still need the new native playthrough.

`python tools/coo/verify_gateway_mainland.py` protects all other source/script files against the installed position-recovery archive, validates package identities and independent native wire fixtures, runs Debug/Release regressions and stages a separate Release DLL. Tests cover the recorded shelf straggler, ordinary shelf clearance, a missed shelf reinforcement volume, world-entry ambient activation, scoped mainland source collisions, required central deaths, optional outskirts, delayed dialogue, and no activity completion. `verify_gateway_mainland_bindings.py` independently checks all 19 new source identities and reflects three representative source bodies (ruled two-category, inline Hydra, and inline ruled two-category).


## Native combat tasks and Forest lattice, 2026-09-06

The accepted mainland run was missing combat tactical joins. A read-only capture
of source runtimes 4B946B28/55 and /63 showed outer task +5FC and inner command
+600 both -1. Marcher 85742F3E/16 already had outer 0 / inner 0. Evidence is in
`build/coo/gateway-ai-lattice/live-source-tasks.json`; these addresses are
historical snapshots and must never be reused as live pointers.

`ai_bindings.h` assigns all 42 combat sources to native type3 groups: traversal
5 (end), 6 (recess), 7 (shelf), mainland 16 (center), and 17 (outskirts).
`tools/coo/generate_gateway_ai.py` checks group/provider identities, extracts
native firing bounds and spawn-rule placements, and reconstructs an initial
outer task by nearest firing bounds with a center-distance/row tie-break. These
joins are reconstruction policy, not recovered retail script assignments.
Marchers retain their existing scoped groups and task 0. Native scheduling
still chooses the inner command and performs movement, targeting and combat.
No native AI routine is replaced and no global AI patch is applied.

The mainland type23/2 sensor (80F46EC9+278, infinite_forest_entrance_device)
publishes position 1, revision 1, snap true whenever the Gateway frame is enabled,
including before the first spawn generation. Power and lock revisions stay
absent (-1). This matches the accepted Omega closed lattice bytes. Gateway
never releases this lattice or requests a Forest transition in this profile.

Regression coverage checks all 86 source assignments, inactive cohorts, exact
native-reflected source bodies, lattice bytes, cross-registry scope, initial
arrival, and persistence through blocked-entry dialogue. The new movement and
visible lattice still require validation in the replacement build's live run.


## Complete gameplay candidate: return fight through Vance

The user accepted progression through the blocked Forest entrance and asked to
finish the mission before revisiting encounter density or adding a universal
combat validation feature. Profile `gateway.ending.v1` preserves the accepted
30-step opening graph and presentation bank, then runs a separate 16-step ending
graph. Outbound volume observations are cleared at this boundary. The Forest
lattice stays closed; this mission ends inside the Lighthouse.

The ending requests 40 additional authored combat sources. Cohorts 9 and 10
populate the center and outskirts on the return trip (19 and 16 actors). The
Lighthouse approach starts wave 1 (four actors), then wave 2 (ten actors), then
the Module Minotaur with six support actors. Counts remain one actor per authored
category, a reconstruction policy rather than recovered retail spawn counts.
The earlier 27 mainland defenders and all traversal counts are unchanged. All
82 combat sources now have authored native tactical groups; existing 42 joins
remain identical. Template choice and inner AI scheduling remain native.

The return dialogue can start after a qualified return-center enemy death or
Lighthouse approach. Both initial Lighthouse waves require real admissions and
deaths. The third wave waits for the Module Minotaur's qualified death; its
support actors do not delay the module objective. Native source 4B946B28/4/32
(80F46F23+4C8) then creates the separate intelligence module. A new bounded,
read-only observer follows the native resource allocation chain and runtime
component metadata (591290, 59A350, 557529..55754A). It accepts health definition
80F48026/80804B8A+B08 only with exact source, entity, serial, health, run and
generation identities. An observed live owner must precede the matching native
health death flag. Boss death, a trigger volume and elapsed time cannot substitute
for module destruction. Object preparation uses the existing inactive-apply
receipt before publishing the gates, shield, laser or module as active.

Module destruction removes the entrance shield and laser and publishes the
entry objective. NPC-registry volumes 11 and 13 establish interior entry and
Vance approach. The catalog extractor now retains the three authored `tv_`
volumes it previously omitted (76 polygons total). Rows 6 through 10 use the
existing native dialogue dispatcher. Source BA0B27A0/1/4 and Scene /43/5 bind
Vance to selector 80EC0ABC. Its blocking child 80EC0AC5 contains rows 11 through
15; those lines are owned by the native Scene, with no duplicate host queue.
The existing B438B0 hook observes the exact Scene owner before and after its
original tick. Only native completion advances the final executor step.

The finished frame retires the Vance objective and requests native lifetime
phase 6 / result 1. These lifecycle values are a reconstruction: reflection
confirms their encoding and the native terminal-state handling, but the mission
complete HUD and staying inside the Lighthouse require the first ending run.
The module metadata path, gate polarity, Vance cast/animation and full closing
voice sequence likewise need in-game validation. No automatic EDZ launch is
requested; the supplied video does not show one. Prologue cinematics outside
that gameplay video and new Gateway checkpoint logic are outside this change.

`python tools/coo/verify_gateway_ending_bindings.py --check` checks all 40 added
sources, ten object/scene identities, the five scene-owned dialogue selectors,
native query instruction anchors, and eight independently reflected wire
fixtures. `python tools/coo/verify_gateway_ending.py` protects accepted AI-build
sources, the complete earlier opening graph and existing AI joins, runs Debug
and Release regressions, builds the DLL, and freezes its source archive under
`build/coo/validation-gateway-ending`. It does not install or launch the game.
Useful live receipts are `module_bound`, `module_destroyed`,
`vance_scene_started`, `vance_scene_completed` and `mission_finished=1`.

### Return-region reset correction (2026-09-07)

The live return run crossed from region 120 to region 128 at log time 696203 ms. The host selected Gateway only in region 120, so its next snapshot reset the controller and dropped Gateway authority. Read-only inspection confirmed run generation 1, world arrived, mission seed armed, and Gateway selected run 0 while the player stood inside finale volume 461.

Selection now follows the owning `mission_abs` activity throughout ordinary region changes. Native participation continues to advertise the real region, and Gateway objects retain their bubble-15 sub-block. Other destinations still reset Gateway; foreign-session snapshots cannot select or reset the local controller. A destination reset is now logged. No population, death threshold, mission graph, or dialogue changes accompany this fix.

Code regression validation and a new native run are required; the stalled process cannot recover the controller state that it already discarded.

## Lighthouse intelligence module lifetime correction (2026-09-07)

The restored `gateway.ending.v1` playthrough reached final boss death and waited
for module destruction, but source `80F46F23/4C8` had requested/committed generation
1 and no entity. Native `9F2F30` requires requested > committed before creation;
reactivating generation 1 cannot recreate that retired object.

Each Gateway run now reserves two generations. Inactive preparation uses the
first; the module requests the second immediately after preparation, at arrival.
Boss exposure changes its target device and damage permission without replacing
the live entity. Destruction deactivates its source; it cannot respawn on a later
authority update. Restart generations advance beyond both prior values.

The module is bound while alive using the observed source address, the native
entity weak reference and the exact `80F48026/B08` health component. Source +24
is not assumed to be a typed component handle. The installed object-source owner also installs damage hooks scoped
to that exact health identity in an active Gateway run: B804E0 rejects early damage; after exposure CDCB60 permits lethal
health changes for the currently bound owner. Native damage, destruction and
visual effects execute normally after exposure. B7E3C0 and the B804E0 return
observe a real dead health flag before entity retirement can hide it; source
sense remains an additional observation path. The controller still requires the
matching live owner before accepting death. Boss death alone never opens the
Lighthouse barrier. The Forest lattice remains closed.

`tools/coo/verify_gateway_module.py` protects the restored region candidate outside
these module changes, replays native generation/health gates in Unicorn, runs
Gateway/protocol/Omega ending regressions and builds a separate Release DLL.
`module_source`, `module_bound` and `module_destroyed` logs distinguish creation,
health binding and confirmed destruction. A fresh in-game validation is required:
box present and immune before the boss, shootable after the boss, barrier opens
only after shooting it, then enter the Lighthouse and continue to Vance.


### Module presentation activation (2026-09-07)

PID 63652 confirmed generation 2 creation and an exact live health binding,
but the user could not see the module. Its target device still received position
0 before boss death. Packaged graph 80F48031 reads the `device_position` property
(FNV-1 6D408B83). Its low-position condition at 37B0 selected the inactive branch
in the live capture; the positive-position condition at 3840 and activation
sequence at 2E20 were inactive. Thus creating the entity alone did not activate
its presentation.

The target device now requests position 1 from arrival until module destruction.
The exact-owner damage hooks continue to reject pre-boss damage. Exposure keeps
the same object and presentation active; confirmed module destruction retires it
and then opens the Lighthouse barrier. This change does not alter either mission
JSON or actor population. Code and wire checks cover early visibility activation,
late damage permission, and barrier ordering. Visual confirmation remains pending
in a new native run; entity creation and health binding alone are not proof that
the model is visible.

## Responsive module and Vance entry correction (2026-09-07)

The first assembled ending run reached mission completion, but its authoritative updates followed the idle 5,000 ms keepalive. Logs record module death at 510125 ms and its next authority at 515063 ms. Vance row 10 dispatched at 535125 ms; the closing child began at 545125 ms. The approach had already been retained at 530063 ms: the script did not require an exit/re-entry, but the delay made it appear necessary.

Gateway now uses the existing mission publication wake-up condition at a bounded 100 ms cadence from module exposure through completion. The opening cadence, shared packet encoding, session structures and delivery commits are unchanged. Both beam source deactivation and device position zero reach the next ending update following the real cube death. Failed attempts remain eligible for a later bounded retry.

The invitation is triggered by interior entry, before approaching Vance. Ghost's short entry line follows it. The native closing scene joins the retained approach and entry dialogue, and waits for the native turn plus the remaining audio window; an approach during the greeting is retained without another entry edge. As with existing dialogue, an earlier line already playing is allowed to finish.

`tools/coo/verify_gateway_ending_response.py` protects the installed integration outside this narrow change, checks the unchanged keepalive flow after removing the single Gateway wake-up condition, and tests early/late single approaches, first-update beam shutdown, cadence bounds, retries and reset. Fresh in-game confirmation of the response timing remains necessary.
