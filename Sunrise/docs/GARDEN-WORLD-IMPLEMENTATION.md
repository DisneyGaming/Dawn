# A Garden World strike implementation

## Scope and evidence

The requested deliverable is the **strike** from `GARDEN-WORLD-STRIKE-20260911.zip`, with an A Garden World entry in the Curse of Osiris mission launcher. The user supplied branching strike dialogue, objectives, a strike video, and a campaign walkthrough. The campaign scanner/Panoptes sequence is not part of this strike. The attached authoring/template documents were treated as reference material; they do not grant separate operational authorization.

Evidence labels in this record distinguish **VIDEO** observations, **PACKAGE** extraction, **OFFLINE TEST** results, **ASSUMPTION** reconstruction choices, and **NOT YET TESTED** live behavior. No offline receipt is presented as a real game event. Earlier live experiments described inside the supplied archive are historical evidence from that archive, not acceptance of this revision.

Reference video: https://www.youtube.com/watch?v=-uxfFqyMtxE . Supporting mechanic reference: https://www.destinypedia.com/Dendron,_Root_Mind . User transcript reference: https://www.destinypedia.com/A_Garden_World . Local evidence resides under `build/coo/garden-world-reference-20260911`.

**VIDEO review coverage:** the whole 19:36 recording was visually sampled at five-second intervals (235 images). Security progression at 6:50-8:20 and the first boss shield phase at 15:25-16:30 were reviewed at one-second intervals. Additional one-second frames cover 17:10-18:05; consecutive-frame windows cover 7:43-7:45, 16:14-16:16 and 18:44-18:46. The full stream audit measures pixel change across all 35,242 decoded frames. `video/all-frames-audit.json`, `all-frames.csv`, contact sheets and per-folder `coverage.json` record the method. Automated processing of every frame does **not** mean every frame received individual visual inspection. Contact-sheet timestamp labels are approximate sampling times.

## Source integration

The archive patch, based on `74d6126`, was merged into the current checkout rather than replacing the user's files. Pre-import copies and the merge report remain in the evidence folder. Existing launcher, logo, layout, Tree of Probabilities and Hijacked work was preserved. One-time merge scripts were archived outside the deliverable source directories and must not be rerun over the completed mission.

`Sunrise/scripts/strike_bond.lua` owns chronology. The new `state/activity/strike_bond` adapter owns typed package capabilities and authentic receipt validation. Shared services retain population, objects, objectives, dialogue, scenes, generation ownership and protocol publication. Native integration reuses existing object, damage, actor, dialogue, Forest and player-position callbacks; no new detour addresses or fabricated player movement were introduced.

The mission is wired through activity selection, registry roster publication, retained-region membership, readiness polling, native receipt dispatch, dialogue selection, forced launch and packaging. Shared task costs were extracted from `strike_pact` without replacing its mission logic.

## Launcher, route and checkpoints

**PACKAGE:** package `strike_bond`, scenario `80F5426E`, root registry `BC279389`, dialogue bank `80F1FEC2`. Lighthouse is bubble15/region120; Forest bubble10/region80; Simulant Past bubble1/region8; Spire bubble17/region136.

The launcher has an **A Garden World** Mercury card in Curse of Osiris, before Omega and after Hijacked. Counts are derived from the catalog. There are now eight CoO entries and nine total entries in the current catalog, including the existing Red War entry.

**ASSUMPTION:** opening spawn set `0232EBCE` was selected spatially from native three-player Lighthouse data near `(123.5,255.5,89.5)`. The exact retail Garden launch selector and orientation were not recovered. It is not copied from another mission. Native checkpoint sets `6F7119B7` in the Past and `2EA8FB98` in the Spire are published only after arrival in the corresponding region. Full wipe/re-entry behavior remains **NOT YET TESTED**.

**VIDEO / authored sequence:**

- Lighthouse defense, opening exchange, Infinite Forest gate and the tunnel explanation.
- Native Forest C procedural islands and Daemon doors, fixed exit defense, then the past gateway.
- Simulant Past arrival and flank encounters; first cannon; radiolaria exchange; successive security cubes and terraces.
- First shielded Minotaur: create native scene, observe actual scene start and AI readiness, expose its cube, release its shield on cube destruction, and require genuine Minotaur death to proceed.
- Interior security sets four and five; the rebuilt-security exchange after the fifth cube; cannon defenders and another shielded Minotaur; final cannon into the Spire.
- Lower Spire encounter and cannon, then the mid-floor Minotaur and cube. The lower cannon must work before the higher-floor cube can be reached. Both mid-floor requirements unlock the upper cannon.
- Roof: Dendron is requested on arena entry as a loose combatant with rule 513 and rooftop task 10. Once he and the intact middle cube are ready, the native intro runs. The cube becomes exposed after native intro startup; its genuine destruction sends 01994745, and native intro completion must precede the first damage phase.
- Dendron health thirds each start a native shield scene and create only that side's two guardian cubes and dormant Minotaurs. Each cube independently releases its guardian. Both authentic Minotaur deaths are required before Dendron becomes vulnerable again. The opposite pair remains absent until the next third. Native body damage is clamped at 2/3 and 1/3 to prevent one hit from skipping these phases.
- Genuine Dendron death interrupts unfinished shield/wave work, reveals the native chest, retracts changing cover, removes respawn restriction, clears navigation, plays the full final exchange, and completes the strike.

The alternate roof guardian prefabs4/5 overlap0/3 in package placement and are not spawned together. The recovered Cabal cannon scene variants are not inserted into the Vex strike.

## Native ownership, devices and damage

Object preparation, source creation, health/AI readiness, native scene start, cube destruction, actor death and dialogue submission are separate observations. Source requests do not satisfy readiness. Stale generations, duplicate admissions, mismatched salted handles, immune cube hits, detached actors and squad teardown do not advance required objectives.

**PACKAGE:** ordinary lens health tag `80F48026`; main-lens health tag `815B5AA3`; health class `80804B8A`, component offset `B08`. The existing damage callback resolves the context's typed health pointer, current source/entity/component, weak handle, owner and generation before applying the exposure predicate. It does not write native health. Destruction requires a previously admitted live lens and its real dead bit.

Dendron is the admitted actor belonging to Spire source3. Health thresholds use the existing verified native fraction getter at `CD6C20` from the post-damage game-thread callback. Exact actor/component identity is checked before and after the read. Server publication does not invoke engine health methods. Neither a zero health sample, the seven-bit BAP actor query, detach, nor squad removal is accepted as a death. Terminal progression requires the existing authenticated native source-death callback.

Nine recovered guardian tether/collection pairs are serialized through native type26/type34 bodies. A shield stays linked while its owned cube is present and intact. These actor shield tethers are distinct from cannon bodies/devices. Block barriers use the recovered `d_vex_block` direction: active0, removed1. Cube exposure uses native device position.75, with shielded1 and destroyed0.

**PACKAGE / ASSUMPTION:** changing roof cover uses all32 recovered native block placements, their devices and avoidance objects. After creation acknowledgement, one of four eight-block layouts is active. A seeded scheduler changes to a different layout every12 seconds and retracts all devices at the ending. Geometry is native; the grouping and12-second cadence are reconstruction choices, not a recovered retail schedule. Collision, animation polarity and avoidance behavior need a live check. Other unused environmental tether/collection assets are not claimed as implemented effects.

Respawn restriction is published through both the shared type35 mission director and the global type17 lifetime source. The latter uses **Spire bubble ordinal17**, not region136. Ending writes the explicit unrestricted state.

## Forest, scenes, dialogue and audio

The Forest adapter requires this mission's current owner, an unfinished enabled frame, native worker class `80804FEC` and exact Forest C configuration `80F4E01E`. The configuration value comes from the archive's historical worker observation. It repairs only that worker's native object-authority bit before generation; island geometry, gateway placement and Daemon rules remain native. Fresh portal connectivity is **NOT YET TESTED**.

Scene-owned guardians require real creation/AI readiness and a started native scene before their cubes become active. Dendron uses a direct combatant request before the middle cube exposes; the authored intro binds that admitted actor and the intact cube. Native guardian scene completion releases dependent graph work. Shield selector casts retain their authored parameters. Visible pose, shield timing and scene exits need a live playthrough.

The normal route schedules dialogue rows0,1,3,5,7,8,9,10,11,12,13,14. Row3 is the return-to-the-Past exchange; row11 is after the fifth security cube; row12 is the Spire Arc exchange; row13 follows the main cube; row14 closes the strike. Branch alternatives are supplied by the native dialogue bank, not concatenated into an invented conversation.

Native submission starts each recovered voice window. Closing submission alone does not complete the mission: the entire maximum row14 clip duration must elapse. An early legitimate boss kill discards older optional queued exchanges, preserving the closing row. **ASSUMPTION:** completion waits until this full exchange ends; the reference's success HUD appears before the final spoken line. Actual audible voice/subtitle playback is **NOT YET TESTED**.

**UNRESOLVED:** the package exposes16 music candidates under native bank `80F1FE3B`, group `4A467C68`, but the phase-to-candidate mapping was not recovered. Candidate enumeration is not sufficient evidence of chronology. This revision does not guess an explicit mission music sequence. No audio playback claim is made from the downloaded video-only stream.

## Validation and delivery

**OFFLINE TEST:** `strike_bond_tests` executes the shipped Lua through the actual parser/executor and drives authenticated test observations across all eight phases and a separate ending. It checks native scene lifecycle, lens identity/exposure/death, health and AI readiness, both shield pairs, required guardian deaths, genuine normal/early boss death, full closing duration, checkpoint publication, cover cleanup, stale generations and reset. The current focused Release replay passed 3106 checks, including native damage identity and retained-tether regressions. The shared protocol suite also passed Garden tether/collection body lengths and shield polarity, explicit director restriction and the Spire lifetime ordinal.

Those observations are test doubles, not engine captures. The replay proves state/serialization behavior; it does not prove a traversable, audible, collision-correct in-game run.

The first broad run exposed a missing Garden callback stub in the isolated player-position test harness. The harness now checks that Garden receives the same authenticated coordinates and publication count as the existing missions. The existing launcher lifecycle suite is included in full validation, and the visual test invocation now supplies its current three input arguments. Mission diagnostic flags also reset between runs. Superseded attempt receipts are retained separately; all final jobs are rerun against stable source.

The frozen full candidate is `build/coo/validation-garden-world-20260911`. Its `results.json`, `failures.json`, `source-manifest.json`, `package.json`, `delivery.json` and, if installed, `installation.json` record final results without changing this source record. Full validation expects109 build/test results, including Debug and Release DLLs. Fourteen historical capture-only suites cannot run because their authentic capture inputs are unavailable; `unavailable-evidence.json` names them. They are not replaced with generated proof.

Packaging includes the DLL, symbols, all existing Lua/config payloads and `strike_bond.lua`, with matching source and payload hashes. The installer verifies the package and installed baseline, refuses a running game, preserves exact prior payloads in a timestamped backup, verifies copied hashes, and restores prior files if copying fails. Installation is not a live-playthrough result.

**NOT YET TESTED:** fresh-process launch, actual native portal connections, player control and cannon trajectories, complete AI populations, device animation/collision and cube hit registration, audible dialogue/subtitles, native Dendron scenes and shield visuals, reward behavior, real wipe/re-entry and unforced completion. Exact launch orientation, enemy multiplicities, music chronology and cover cadence retain the limitations above.
## Roof and retained-tether correction, 2026-09-11

The live manual spawn and task-only experiments did not make Dendron attack. The integrated candidate replaces scene reservation with full native loose-spawn authority, including the authored rule and a fixed central rooftop task. Native shield selectors remain responsible for shield presentation, while the owned body damage hook enforces the phase boundaries and genuine guardian death joins. Automated replay and native identity fixtures verify sequencing and damage ownership; Dendron firing and shield animation still require confirmation in a fresh game run.

All three route tethers now write visibility zero on verified cube death, both immediately from the existing damage callback and when a retired source reports a retained render entity. Cached identities are revalidated against current generation, salted entity, bundle and exact beam component before either channel is touched. Retirement cannot re-enable a beam. The native 10 m beam and measured shield endpoints preserve the earlier alignment and thickness correction. Creation checks that the replacement beam definition resolves before entering the shared factory; this guards an unavailable definition but does not establish the root cause of every recorded factory crash.

The existing upper volume 406 disable, launcher card, route cubes and first-three Minotaur release events are retained. No player-health or global kill-volume override is introduced.


## Shield regression correction

The next fresh run stalled on `past.pf_anomaly[1].o_nomaly.ready` (source 150) for at least 40 seconds after both real cubes and the Minotaur had bound. The Lua prerequisite kept the linked cube immune until an optional render object acknowledged creation. The beam template was the Spire-only 10 m class 80F4B0CF, although the first two encounters are in the Past. A directory lookup does not stream that class.

Route cube exposure now depends on native guardian readiness and the request to create its beam, without waiting for the visual object. The first two beams use the already-authored Past center-line class 80F4B0EE with its 75 m native length; the Spire retains its 10 m class and thicker appearance. Both classes and their skeleton lengths are checked before factory entry. Real cube death still retires any existing beam and prevents a late pending creation. The replay now includes a whole run where all three beam creations never acknowledge, proving that the cubes, Minotaurs and remaining mission still progress. Live visual confirmation remains outstanding.


## Native boss startup and cover correction

The next playthrough confirmed the route shields, health thirds and phased guardian pairs, but Dendron appeared only after cube destruction and did not attack. The direct source request was incorrectly attached to the cube-death step. Native intro 80F45CA5 was absent from the shipped Lua.

A qualified live snapshot from PID 10156 proved that Dendron had a valid AI component 808082EC and a requested/applied rooftop task 10 with a real tactical group. The missing AI was therefore not a missing component or an uncommitted assignment. The earlier intro retry was performed after its cube had already died: the captured selector's own cube branch emitted exit event 01994745, despite the external event being held. Boss source/member, cube and static point parameters were bound. This did not prove an intro construction failure.

The corrected sequence requests Dendron on arena entry, waits for real boss and cube readiness, starts the native intro, then exposes the cube after its started receipt. Cube destruction sends the authored exit event. The controller rejects an early exit and refuses the first damage phase until native intro completion. Direct source creation stays in place; the native scene no longer needs to spawn him. Static points remain excluded from the runtime ownership list, preserving the earlier cleanup-crash correction.

All 32 cover blocks and avoidance objects already acknowledged creation, and native device positions were cycling. The previous all-block readiness explanation was disproved by live reads. The actual commands used snap=true: native DF6C70 immediately copies the target position and skips interpolation startup. Only the 32 rooftop cover devices now use snap=false; gates, lens shields and the separate boss platform keep their current values. The separate platform's source and device were already enabled and applied; no additional unsupported platform link was added.

Focused Release replay: 5,219 checks passed, including the native intro/cube/exit/completion ordering, retained phase gates and route shield regression, and serialized interpolation policy for every mission device. Evidence is in build/coo/garden-boss-ai-fix-20260911 and build/coo/garden-cover-ai-20260911. The boss had despawned before a live test could be performed. Native attack behavior and visible cover animation still require a fresh playthrough; exact internal semantics of the paired intro actions are not claimed as recovered.


## Dendron firing investigation — 2026-09-11 tracing build

The saved admitted Dendron character has firing suppression at `+AC4=1`, with
`+AC0` equal to the native one-tick float (`35C75F6C`). The first weapon eligibility
callback `C31200` rejects that state. Saved samples do not establish which caller
refreshes it or whether the character timer update is missing. Clear graph ownership
flags do not exclude an inlined graph writer. No config/idle/target substitution is
justified by the existing evidence.

The diagnostic DLL adds read-only callbacks at `BC8F20`, `BC8F80`, `BCD330`, and
`C31200`. They preserve native arguments, byte results and one original call. The
trace qualifies the exact Garden boss source, lifecycle generation, salted actor,
entity, character reference and component identity on both sides of the callback.
It records suppression before/after, duration or frame delta bits, caller return
RVA, first-caller stack, native eligibility inputs/result, and aggregate counts.
Logging is bounded to 2048 records per run. No suppression, targeting, command,
health, shield or scene state is changed by this diagnostic code.

Validation: ownership and rebinding fixtures plus the Garden mission replay and
Release DLL build. The next live run must distinguish repeated refresh, inlined
writers, zero delta, and missing character updates. This build is instrumentation,
not a verified repair of Dendron's aiming/firing.


## Dendron named animation release — verified live cause, 2026-09-11

The instrumented run proves that native `BCD330` decrements the firing countdown
normally with positive delta. Caller `F4F0D7` immediately resets it on each frame.
The retained action is opcode `39` in Dendron's `815B5A49/808069EE` motion component:
`80F459AD`, machine 1, state 0, a looping presentation animation that forbids firing.
The authored roof scene completed without cancelling this separate named selector.

After the existing intro-complete / middle-cube-dead combat gate, the post-character
update boundary sends exactly one native `C693F0` stop request for group `AFB11A12`,
sequence `31A03F93`, opcode `5D`. Live `80F459CD` maps these to group 0 / selector 1;
interface `80BFDE65` dispatches to `10D35D0`. The engine clears that selector and
notifies the motion scheduler; normal scheduling retires opcode 39 and the normal
character timer expires firing suppression. The hook does not write AC0/AC4,
force targets, bypass eligibility, or free native animation allocations.

Qualification pins the admitted source lease, salted actor/entity/components,
controller-to-selector runtime dispatch binding, exact active startup payload,
named group/sequence lookup and native callback/prefix. It runs only in stage 0
after intro completion, and holds no diagnostic lock across native execution.
The trace records `garden_intro_release stage=selector_released` only after a fresh
native selector inactivity observation. A fresh game run must still confirm aiming,
firing and later boss phases; unit fixtures cannot establish live combat behavior.


## Dendron primary target binding - 2026-09-11

The next live run confirms the named intro selector release: Dendron tracks the
player, the native firing suppression expires, and first eligibility `C31200`
passes. A separate gate, `C30F60`, still rejects his weapon before aiming-solution,
range, cooldown or projectile execution. The live primary target slot contains a
valid player, but opcode `24` decodes target slot `FF` (no target), setting the
weapon request/applied mode to `2/FF` with flags `+154=1`. Authored weapon policy 3
has zero flags and rejects that combination.

Expression 384 in program `80F56183` reads parameter `64F350F0` (token `0x50`)
from environment `80F66F52`, where value index 5 is `FFFFFFFF`. This is an authored
unbound target parameter. The repair binds that exact request to native primary
target slot 0 only for the admitted Garden boss after the middle cube starts
combat. A nested native dispatcher/decoder scope (`C613E0` / `C5FEF0`, caller return
`C6157B`) checks the request bytes, program/environment, source generation, salted
actor/entity/component identities, and a living native target before and after
decoding. It replaces only the local decoded `FF` result. Shared assets, native
eligibility, line of sight, range, charge, cooldown, health and shield logic remain
native or retain their existing mission rules. Another NPC, another request, a
valid decoded target, or a stale binding keeps its original result.

The pre-install read-only validator accepted the production qualification against
live PID 56732, controller `74F9E611`, player target `4DFAA283` (no process writes).
The candidate and Release/Debug test receipts are recorded under
`build/coo/garden-boss-target-fix-20260911`. Regression fixtures cover the exact
request, phase/lease restrictions, target death/reuse, read failures and rebinding
within traversal. Native logs record `garden_target_binding` with
`primary_target_selected` or `context_rejected`. Actual projectile firing and later
combat phases require a fresh run of the installed candidate; this correction is
not evidence that subsequent native firing gates have passed.


## Cached target command replay - 2026-09-11

The next run loaded target-binding DLL `976CBD3F...86CA30B`, but recorded zero
`garden_target_binding` events. The intro release succeeded and all118 recorded
combat eligibility samples retained requested/applied target state `02/FF`.
The live request and native primary target still passed production qualification.

Native `C729F0` and `A7AFE0` explain why: the weapon dispatcher is an add/change
callback. An unchanged opcode24 remains cached and is not dispatched again when
the mission enters combat. The previous correction waited for an event that
combat admission does not generate. This is a timing defect in that correction.

The existing `C31200` hook now replays the exact active cached opcode24 after
native eligibility passes and before `C306A0` consumes the requested target.
It qualifies cache class29, bounds and complete compact row, matching pending AI
request, current actor/AI/source ownership, and native primary target. `A91FC0`
builds the native0x30-byte decoder context from main state+C0; the local context
uses add-action phase4. After another complete binding check, `C620F0` dispatches
the original request. The existing nested target decoder correction then applies
and the native dispatcher commits requested target mode1/slot0. No requested/applied
weapon state, firing eligibility, cooldown or projectile flags are written by the
replay code. Native calls run without a diagnostic lock held.

Replay checks run at most once per second and stop once the native requested
state changes from02/FF. Actual replay attempts are bounded; transient missing
targets remain retryable, with separately bounded guard logging. Logs distinguish
`garden_target_replay` guard/context failure, unconfirmed dispatch and
`target_committed`. Existing firing traces can show the subsequent native applied
mode/slot and attack-state transitions.

A read-only production replay validator accepted the current live actor58F42026,
controller7DF9F5FE, target48FAA26E and cached row2DC62CBE528. Evidence and candidate
receipts are under `build/coo/garden-boss-target-replay-fix-20260911`; the originating
logs and memory capture are under `build/coo/garden-boss-target-followup-20260911`.
Regression fixtures cover changed/retiring cache entries, boundaries, AI ownership,
duplicate or missing pending actions, and mutation during qualification. Native
redispatch and actual projectile firing require the new DLL in a fresh run.


## Integrated Dendron intermissions and death - 2026-09-11

The live test confirmed native groupAFB11A12/sequence8FB6C339 for folding and waking. C620F0/opcode5E gates1B8AB5A9 and71E9ABF9 release the sequence. Both references are removed with C693F0 before the next cycle. Entity80F45BA6, from shield scene80F45CA0, is the user-confirmed burst; native104DA00 dispatches it at the current authenticated biped. Native sequenceFC3C74B9 plays death clip80F459BE and produces the genuine source-death event. A zero-health live test completed the mission after this sequence; no mission-dead bit was fabricated.

The controller now holds damage during parking, guardian combat, and wake-up. It waits for the actual platform position and active folded selector before exposing that side's cubes. Both owned guardian deaths request wake-up and the burst once. Actual selector completion resumes damage and rotation. Parking/wake receipts persist, so sustained fire cannot skip a brief awake state and strand the script. The controller follows native endpoint arrival when reversing motion, including a partial lap resumed after parking.

The user's S/P1/P2 thirds map to native positions0,1/3,2/3. The existing native bone1 attachment stays in place. Movement uses native device interpolation, not actor transform writes. At final zero health the driver captures and freezes the current native device position, persists that position through authority, and starts the native death sequence. The platform remains present and stationary, as requested. Only the authenticated source-death callback releases rewards and ending dialogue.

Confirmed live before integration: attachment/movement, folding, wake animation and resumed attacks, burst, death animation, and genuine mission completion. Full integrated automatic timing and exact P1/P2 beam alignment require the next fresh playthrough. The separate large-shield shatter effect was not established by the burst-only test; no guessed effect was added. Existing route Minotaur cube/beam cleanup, upper-height-volume disabling, boss aiming/firing, cover animation, and health gates remain included.

The two old Dendron intermission scenes are no longer started alongside the new driver. This avoids two native controllers competing for the same named animation or spawning duplicate bursts. Their authored burst resource remains the source of the tested effect. Each guardian still uses its own native scene; both authenticated guardian deaths release the controller-owned intermission.


## Live intermission correction, September 11 evening

The first integrated run rejected every cycle update because its boundary check expected untouched DF6C70 bytes. The already-installed Lighthouse diagnostic detours that function. The platform reached its host-requested target, but no native sleep or parked receipt could be emitted, so the Lua guardian wave remained waiting. The corrected boundary accepts either the exact native prefix or the exact owned Lighthouse relay and original trampoline return. Foreign replacements, changed suffixes and incorrect return addresses remain rejected. Boundary failures now have a separate log event from component identity failures.

The native platform animation covers two revolutions over device input 0..1. Equal-third inputs were an incorrect parking assumption. In PID16376, native position0.375 placed the upper socket at(1595.9567,1150.6345,250.0813), angle14.987degrees; the first authored beam is14.798degrees. The user confirmed this is the correct first stop. Position0.625 placed the socket at(1491.2682,1122.6096,250.0846), angle-165.014degrees; the second beam is-165.085degrees. The socket lies about4units behind the beam start, at the boss body rather than its face. Both stops use these measured inputs. Temporary callbacks were restored after each native test. No actor transforms, health or AI fields were edited.

Regression fixtures include the actual E9/FF25 detour shape, its trampoline return, and rejection of foreign or altered code. A fresh complete encounter still verifies the integrated sleep/guardian/wake/death sequence.


## Immediate normal-speed parking and centering

The user clarified that a health gate must start turning toward the stop immediately at normal speed; it must not teleport or accelerate to the destination. The previous run started at device0.860230 and incorrectly targeted0.375, taking29.25seconds through nearly a complete extra orbit. Because the native input covers two revolutions,0.875 is the same physical stop nearby. The controller now records the nearest equivalent target at the health receipt and publishes it immediately. The native cycle driver also starts that normal-speed command once on its existing admitted-character boundary. Near an input endpoint, it can rebase by exactly0.5 to the equivalent physical pose before the normal-speed turn; the destination itself is never snapped. Captured-run travel becomes under one second at the unchanged native speed. Other starting positions still take the normal travel time over their shortest arc.

P1 now uses0.3758/0.8758 to center Dendron's body and eye, rather than only the platform socket. The user confirmed centering in live PID61084. P2 uses0.6259/0.1259, applying the same body offset against its slightly different authored beam angle; its one-shot live adjustment is recorded separately. The parking target remains fixed while asleep and waking, and normal DPS movement resumes through native sequence completion. Arrival still requires both the real dormant animation and the observed stationary platform; no fabricated arrival or guardian-spawn receipt is introduced.

Focused regression tests cover both phases across101 input positions, valid native bounds, equivalent-pose rebasing, absence of an extra orbit, immediate host publication, persistent parking and the existing full mission replay.

Death completion correction, verified live in PID61084: treating every non-damage mode as immune also blocked the authored death animation's native kill event. Restrict that mode check to parking/dormant/waking. The dying mode keeps native damage eligibility, while real bossDead still blocks damage. After changing only the three mission damage predicates and replaying FC3C74B9 once, C72390 delivered death_accepted for actor5DF42013 and the mission entered ending. The user confirmed the death animation looked correct. The platform remained stopped.


## Current integrated behavior — issue report follow-up

This section supersedes the earlier continuous-lap and wake-up-immunity descriptions.
The user's final scope keeps the existing two-thirds/one-third health gates and both
rooftop guardian pairs. Infinite Forest generation and the other Minotaur redesigns
are deferred. Only the three route Minotaurs gain AI while shielded.

Dendron and his native platform/intro are prepared during the middle of the climb,
before the upper cannon opens. The rooftop objective points to the central cube.
Breaking that cube releases the retained intro hold, then plays the native opening
sequence 31A03F93 with its authored terminal exit input 9CD3EB24. The old hold cleanup
cannot cancel this new sequence. Cover waits for real opening completion.

The opening damage phase stays at spawn. Each health gate immediately starts a
normal-speed shortest-arc turn toward its confirmed, centered P1/P2 stop. Sleep,
stationary arrival, both guardian deaths, wake-up and burst keep their native
receipts. Once both guardians die, Dendron takes damage during the whole wake-up
transition. The next health floor still clamps damage. If that floor or final zero
is reached during the clip, its transition is consumed when the clip finishes;
no extra shot is required, and competing sleep/death clips cannot interrupt wake-up.
After wake-up he makes one trip back to the equivalent spawn position and stays
there until the next gate. Death freezes the platform and retains the corrected
native kill-event eligibility and authentic mission completion.

Route guardians C95ECB1A:105/121 and 2CB86C0F:244 receive their existing native
33E63A8B scene-release input once their actors and scenes are ready. Cube exposure
waits for that scene completion. Their independent type-26 shield still resolves
the same source and remains enabled until its own cube dies. Cube placement,
shield linkage, transporter locks, short-beam cleanup, and rooftop guardian
release timing are unchanged. No raw actor-AI flags or transforms are written.

Presentation corrections: the exit approach shows Enter the Past; Past arrival
shows Enter the Spire; the radiolaria exchange queues with Sabotage; dialogue11
follows block3 rather than block4; the Arc-energy exchange uses an interior volume;
and the roof's sabotage marker targets the central shootable cube. Missing generic
strike exchanges4/6 are now included. Campaign-only row2 and scanner/Panoptes
rows15–23 remain excluded; the strike still ends with dialogue14 and genuine death.

The report's music-publication fix was absent from this checkout and is included
here. Region and boss progress monotonically raise the native sensor candidate;
only authenticated death starts the closing candidate. The report's ordinal map
2/4/6/8/10/12/13/15 remains an explicit reconstruction that needs listening checks.
It is not a recovered retail soundtrack-to-phase mapping.

Validation and build/install receipts: build/coo/garden-issues-20260912. Focused
Release and Debug replay tests cover early route-AI release with a retained shield,
stationary opening, roof marker/cover timing, both gate/wake cycles, damage during
wake-up (including next-gate and zero-health hits), genuine death, music publication,
and existing route/tether/launcher behavior. The newly integrated opening animation,
station-to-station motion and shielded route-guardian attacks need a fresh native
playthrough; older successful live tests do not prove this new timing.


## Route AI release dependency correction

Fresh run64852 confirmed route Minotaur AI activates after33E63A8B, but the
scene does not finish while its cube remains intact. The previous Lua waited on
sn_golem.finished before requesting the short beam and exposing the cube. Logs
show terrace scene110 still waiting after160seconds: cube111 had a live binding,
but no exposure or tether request could execute. This was a dependency deadlock,
not a failure to enable the Minotaur AI.

Route AI release, short-beam request, and cube exposure now proceed from the
started scene/ready actor without a scene-completion join. Cube destruction
still retires its shield and both beams; the source's authentic death still
unlocks the transporter cube. Rooftop Minotaur behavior is unchanged. The regression
replay now deliberately leaves each route scene unfinished until its cube dies,
matching the live observation instead of inventing completion on the AI input.

Evidence and installation receipts: build/coo/garden-route-ai-link-fix-20260912.


## Dendron visible shield (2026-09-12)

The large shield shares Dendron's mesh model, rather than the separate cube
visual. Live testing confirmed that suppressing native draw pass 7 removes
the shield while retaining his body. The existing character update now queues
that pass through native 1150420, using the same boss_blocked predicate as
damage eligibility. It is hidden during damage and both wake-up animations,
shown during opening/parking/dormant immunity, and hidden before the native
death animation. This does not alter damage, AI, movement, or other draw passes.

The bridge validates the current boss lease, character, model and health headers,
renderer handles, model subobject, and native entry bytes before dispatch. It
resolves components again immediately before the call. Native pass counters
are preserved; showing restores their current effective count. A bounded
250 ms refresh survives native visual updates, and state changes are checked
every 50 ms. The dying update hides the shield before starting the death clip.

Evidence, focused checks, build and installation receipts are under
build/coo/garden-shield-visibility-20260912. The manual shield-only toggle was
visually confirmed; automatic full-fight transitions require the next fresh run.
The installed Infinite Forest endpoint correction is retained.
