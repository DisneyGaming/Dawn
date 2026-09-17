# A Deadly Trial: native test candidate

Current scripting reference: [Lua mission authoring](LUA-MISSION-AUTHORING.md). Mission JSON has been retired. Dated captures, hashes, and acceptance notes below describe the builds in their cited evidence directories; use the current installation receipt for the active DLL.


The supplied objectives and transcript define the requested mission. NEXT-MISSION-HANDOFF.md supplies project context, not an additional user request. The linked video could not be fetched and was not watched. Native identities and geometry come from the installed archived packages.

## Launch

Select **Activity override > A Deadly Trial opening**, then launch **Chosen**. The prelaunch profile replaces donor activity 282 with **293** before native launch data is derived.

- Package `adventure_ginger`, investment hash `87D9CA16`, root registry `C9BC773A`.
- Activity `80B2E004`, scenario `80B2E043`, launch descriptor `80FDB97F`.
- EDZ town bubble **51**, packed region **408**, spawn set **43954D08**.
- Three spawn points near (595.5, 144, 74) lie in the central exclusion of the authored opening dialogue filter. Moving out into that filter arms the mission. This is a spatially recovered candidate; exact retail spawn selection and orientation need a live check.
- Ownership survives town/alleys_a/alleys_b, regions **408 / 0 / 8**. Twelve verified roster groups retain their native bubbles and sparse slot identities. Actor and heap limits are unchanged.

## Executable progression

1. Opening filter: rendezvous objective and native dialogue row 0.
2. Square entry: hub sources 1-3 in EB7AF018. Require all three authentic deaths, then show Pike objective and enable native hub Pikes 4-6.
3. After Square clearance, an authenticated local-player Pike mount or the retained Followers road filter releases dialogue row 1. Street/chokepoint volumes request separate resistance cohorts. Their survivors do not become additional kill gates.
4. Overpass arrival: roadblock objective, support, **Walker source 23**, and one Skiff owned by source 34/member 35. Retained authored downstream arrivals release missed narrow road filters without satisfying any kill gate. Only the admitted Walker actor's authenticated native death clears the roadblock and activates all Radio Tower sources 25-39. Position, elapsed time and support deaths cannot substitute.
5. Open barrier device 46, enable overpass/cliff Pikes, submit row 2 at the post-Walker filter. Cliff and tunnel triggers request their native troops. Tripwire device 14 remains active.
6. Radio tower filter: row 4 and clearance of the tower sources already activated by the Walker's death. Require those actors' deaths before Search the temple.
7. Lair volume 195: sources 53, 55, 57, 58, including cloaked Marauders. Require four deaths. The bodies filter triggers row 7 and Revive Sagira.
8. Create native pedestal source **EB7AF01B/type 4/59**, definition **80B2EBA7**. Bind its generation, source, weak entity and controller. Enable the unused native interaction through F33930 / 80804FB8 mode 2, preserving its authored predicate. Require actual local-player hold consumption at F36640. Room entry cannot revive Sagira.
9. After the authenticated hold and preceding Followers line, summon the local player's Ghost, release the holding pose and bind it to the native revival device. Publish **27660927/type 65/0**, definition **80B2E6D7**, using the 65-bit 80804D3F authority. The animation itself dispatches row 9 at 0 seconds and row 10 at 10 seconds; both rows are marked scene-owned and are excluded from the host queue.
10. Publish completion state 6 only after native scene completion and the final dialogue tail both finish. The tail is measured from a witnessed native animation cue, not a second host audio submission.

## Reconstruction policy and native boundaries

The candidate requests one actor per native category: **47 sources, 58 actors**. These counts are reconstruction policy, not recovered retail density. Square, Walker, tower and lair kills are required. Street, overpass support, cliff and tunnel survivors are optional. Alternate waterfront/coastroad encounters and unrecovered dropship flight paths are excluded. The overpass Skiff is enabled once with matching source/member generations and zero loose requests. It remains hovering; its attack/departure choreography is deferred. Native templates own Walker leg/core damage and Marauder cloaking.

Tactical identities, firing bounds and placements are native. Assignment among valid tactical rows uses nearest firing bounds, then area center; evidence is in `build/coo/deadly-trial-research/tactical-joins.json`.

Pikes/pedestal publish inactive generation N, then create at N+1. The lifecycle reserves both generations. Old runs, generations, serials, unrelated controllers and unconsumed requests are rejected. The dedicated E4A590 callback validates the current run, native component, group/sensor, owner serial and bound Ghost before accepting playback receipts. The live barrier test established position **0=closed/visible, 1=open/removed**. The encoder now publishes those values and changes to open only after the real Walker death gate. Tripwire position 1=active remains a reconstruction assumption. Publication alone is not native controller acknowledgement.

Dialogue bank **80F1F086** supplies the given exchanges. The host owns rows 0, 1, 2, 4 and 7; optional hints 5 and 8 are not queued. The Ghost-link owns rows 9 and 10. The old type-43 scene is no longer published. The original integrated Ghost-link build still queued host copies; the dialogue-ownership correction below removes those copies. Native submission receipts start voice windows. An unacknowledged required row stalls with diagnostics instead of silently completing.

Objectives come from **80B2ED24**. Markers now use authored ActivityPoints (type 47), complete destination/bubble/registry/point locators, and native route-point kind 3/display mode 2 for the small persistent diamond. A scoped native presentation bridge updates objective text after transitions and supplies requested dialogue before the native scan. It validates the component identity and current run; native submission still controls voice-window timing. Continuous traversal breadcrumbs between authored goals have not been recovered. Static tower geometry and follower corpses remain authored world content and require visual confirmation.

Reset clears both executors and retires observations/bindings. The generation high-water mark survives reused run identifiers. This candidate restarts from the opening; live checkpoint and death/retry behavior are unverified. Automatic next-mission launch is not implemented.

## Evidence and tests

- `build/coo/deadly-trial-research/native-bindings.json`: native groups, slots, dialogue, objectives, volumes, sources, placements and source hashes.
- `tools/coo/recover_deadly_trial.py`: native recovery entry point.
- `tools/coo/generate_deadly_trial.py --check`: read-only verification of generated catalog, graph and capability contract.
- `Dawn/scripts/deadly_trial.lua`: Lua mission definition, loaded once per process.
- `Dawn/unit/deadly_trial_tests.cpp`: full progression replay, withheld Walker death/interaction/scene completion, stale/duplicate receipts, reset, graph-bypass rejection, multi-bubble roster and wire checks. The zero-cast body is compared byte for byte with the independent Omega native scene encoder.
- `build/coo/validation-deadly-trial-candidate-20260907/`: final build and test evidence. The earlier failed object-name-collision build is preserved separately and was not installed. New translation units now have unique filenames.

Gateway and Omega scripts remain byte-identical to the accepted handoff. Gateway retains its 8,960 ms cue. Original regression assertions are unchanged. Unit tests establish host and packet behavior, not visible or audible parity.

## First live check

Check arrival/fade/roster, Square/Pikes, Walker legs/core and barrier removal, route/tripwires, tower/shaft/Marauders, pedestal hold prompt, dialogue once, revival visuals and completion. Report the first divergence and retain that process's log. Diagnostics use `ev=deadly_trial` and `mission=deadly_trial`. Installation evidence preserves a DLL/script rollback pair. Do not replace the DLL while Destiny is running.

## First live launch: Baboon and roster correction

The first candidate reached native world entry and released the fade. At t=88,484 ms the client requested the normal region transition 408 -> 0. At t=88,531 ms, the server's preliminary roster had only four groups; mission root C9BC773A was absent. The mission adapter rejected that snapshot, stopped delivering updates, and the local activity connection subsequently timed out. The user reported Baboon with no playable mission.

The captured native cache has two authored overlay entries for town (51), but zero for both alleys (0, 1): their complete overlays exceed the generic five-entry cache capacity. The correction prepares a verified root-only overlay before the normal roster builder. The existing mission adapter then appends all local groups in a constant order under their original bubbles. Town's preliminary local entry is normalized too, preventing a changed group order at the transition. No cache or capacity limit is modified.

The regression reads `unit/fixtures/deadly_trial_launch_layout.bin`, captured from that failed process's cache, through the production cache decoder. It checks the missing original alley roots, restored identical overlays for 51/0/1, stable ordinary groups and bubble masks, unchanged map/spawn data, cache-index relocation, invalid roots, and repeat application. Existing mission and wire regressions remain required. Evidence and the saved failed log are in `build/coo/deadly-trial-baboon-20260907/`; corrected build evidence is in `build/coo/validation-deadly-trial-roster-fix-20260907/`. The subsequent live launch reached the Square and overpass. The user confirmed opening dialogue, Square enemies, the small persistent marker, Walker spawn, visible barrier, and eventually a stable Skiff. Further problems and their fixes are recorded below.

## Consolidated live-fixes patch

Build/installation evidence: `build/coo/validation-deadly-trial-live-fixes-20260907/`. The patch carries the roster correction, downstream road-trigger handling, scoped objective/dialogue delivery, authored small markers, correct barrier direction, and stable single-member Skiff activation. Experimental Skiff flight orders were removed. The shared source codec now has an explicit member-owned option that preserves the native parent defaults observed in the stable live test; existing loose-source callers retain their previous bytes.

The user killed the Walker during live testing. The host accepted the real death, completed the roadblock/barrier steps, and selected the radio-tower objective. The native HUD had retained the opening objective. A temporary live bridge then changed the native event to E58BB2F6 and restored marker kind 3/mode 2; this was verified in memory. The user stepped away before visually confirming that final transition and the game subsequently closed. This patch ports the presentation fix into normal compiled code; it contains no process-specific pointers or temporary remote hooks.

Debug/Release mission tests cover complete host progression with withheld real-death, interaction and scene receipts. Native Skiff source/member wire fixtures come independently from the pinned executable reflection/default programs, including equal generations, zero loose requests and an empty command queue. Regression builds cover Gateway, shared services, other-mission protocol and frozen Omega authority. Exact results and hashes are recorded with the build.

A new integrated playthrough is required. Test opening, Square clearance/Pikes, the normal road to the Walker, barrier removal and the radio-tower objective, then the tower/shaft/Marauders and Sagira interaction through completion. The ending and exact enemy density have not yet been accepted live. Skiff flight-out is deliberately deferred until progression testing is complete.

## Live continuation, Pike and pedestal (7 September 2026)

The user confirmed that the live Pike mount check advanced the objective and that road markers and encounters worked. The Walker died through the normal death path; barrier progression, cliff, tunnel, tower and the four lair enemies then advanced without fabricated kills. Two full-health tunnel-exit guards initially held the tower clearance gate; their later native deaths released it. This candidate still requires all tower sources 25-39.

Mounting removes the Guardian's foot physics body. The controlled character remains the same, but its native transform parent becomes the drivable vehicle. The vehicle's `808071BC` driver-seat component (`80FDA97D`, definition offset D8) names the Guardian at +48. Validate full entity/controller identities before and after the native world-position read. Sample that position for Trial's volume observations while mounted. The mount receipt is retained only after Square clearance, is scoped to the run/spawn generation, and cannot release later area or death gates. Player/vehicle metadata can exceed the enemy iterator's previous 256-row bound; the mount probe explicitly allows 1024 with the existing byte budget. Other component callers retain their previous bound.

The pedestal was created and bound but its interaction remained blocked (+2C0=1). The live native F33930 enable command cleared that block, retained the predicate, and left use/request/consumed counters unchanged. Prompt and revival-sequence user validation is still pending at this checkpoint. The candidate DLL includes this source-qualified enable command in the existing interaction callback.

Live tools and captures: `build/coo/deadly-trial-pike-stall-20260907/`. Candidate build and tests: `build/coo/validation-deadly-trial-pike-20260908/`. Destiny process 53352 has two temporary scoped bridges (mount observation and native pedestal enable); the installed on-disk DLL remains the previous 332af725 candidate. Do not replace it while the game is running.

At this earlier checkpoint the user deferred the exterior tower marker and Skiff departure. The exterior marker still targeted underground `80B2EC53`. The marker was subsequently reopened and corrected in the 8 September live test below; Skiff departure remains deferred.

## Rerun: Walker death activates the tower (8 September 2026 UTC)

Installed rerun evidence is in `build/coo/validation-deadly-trial-rerun-20260908/` (DLL c8eeab4a). Process 69320 accepted the Walker death at t=181109, then visited cliff 157, tunnel 223 and tower BC1C972B/2. The post-Walker dialogue filter 4DDB8242/2 was not visited. The sequential graph waited at `trial.entered`, leaving cohorts 6-8 disabled even after tower arrival.

The user explicitly requested that the final exterior encounter spawn regardless of downstream progress and be connected to the Walker dying. `Controller::died` now activates all 15 tower sources immediately after accepting the current Walker's actual death and verifying cohort clearance. This is independent of road observations, voice submission, and objective dispatch. The later tower clearance command retains the same generation and ledger; it cannot respawn killed actors. Tower kills, lair kills, the pedestal hold, and scene completion remain required.

Retained authored cliff, tunnel and tower arrivals release missed earlier post-Walker travel observations. This does not write invented volume visits or satisfy kill/dialogue receipts. Tests cover direct Walker-to-tower activation with every downstream observation withheld, invalid death receipts, missing filters, preserved kill gates and reset.

Live evidence and the process-scoped bridge are in `build/coo/deadly-trial-tower-stall-20260908/`. The current Walker's accepted death was already recorded, so the bridge activated the existing tower cohort through the compiled controller and released the missed travel wait. Native tower actors and dialogue row 4 were observed afterward. Visual confirmation is pending while the user returns to the tower. The installed DLL is unchanged during this live run; the updated compiled candidate must be installed with Destiny closed. Skiff departure remains deferred; exterior waypoint validation is recorded below.


## Tower arrivals and accepted exterior marker (8 September 2026 UTC)

The paired post-gate road directive `4DDB8242/1` was observed at t=196187 while dialogue filter `/2` was missed. It now releases `trial.entered` as an alternate, before the cliff/tunnel fallbacks. Native row 2 owns Ghost's "I feel bad for anyone who has to live out here" and Ikora's trial response. The primary gate filter remains valid. The alternate road directive is farther along the road; exact audible distance from the gate still needs a fresh normal-route playthrough.

Tower arrival accepts the authored approach `C91BDFF0/1`, exterior `BC1C972B/1`, or existing dialogue filter `BC1C972B/2`. The rerun entered the first two about five seconds before the narrow dialogue filter. This requests native row 4 ("It really is just a radio tower" / "That's what faith in Osiris gets you") and releases any missed earlier travel filters. Each row remains queued once and respects the preceding voice window. The objective stays on Follow Ikora's coordinates during the fight and updates to Search the temple after all required tower sources die. Neither arrival nor dialogue submission counts as a kill.

The exterior E58BB2F6 marker now uses authored point `C91BDFF0/47/4`, definition `80B2EC47`, and its full native locator. The authored position (1374.614624, 544.469482, 244.565933) placed the diamond too high; the user confirmed this during the live test. Lowering Z to **198.662613**, the center of the authored entrance directive's height range, was accepted: **"Height looks right."** This is an explicit presentation adjustment, not the recovered point's original altitude. Kind 3 / display mode 2 retains the small persistent diamond. Search-the-temple and Revive-Sagira objectives retain their separate underground targets.

Live acceptance: `build/coo/deadly-trial-tower-stall-20260908/user-marker-validation.json`. Compiled candidate: `build/coo/validation-deadly-trial-tower-arrivals-20260908/`. The current process retains the live tower and marker bridges; the durable candidate contains ordinary compiled code with no process-specific addresses. Fresh-run dialogue timing and the Sagira hold/scene/completion still require user playtesting.


## Accepted revival and combined patch (8 September 2026 UTC)

Live tests recovered the actual Ghost-link owner, `80FBDA45 / 80804D3A / 358`, behind component `80B2E6D7 / 80804D32 / 258`. Publishing the old type-43 selector alone produced a static Ghost, missing Sagira and repeated scan dialogue. Its parent did not terminate after its visible children. The successful replacement uses the native Ghost-link independently of that parent.

The permanent callback follows the native action at 10A5370: resolve the controlled Guardian and its 808061CF spawner interface; summon with B72E40; wait for the real Ghost's mode 1; call B737D0 to release the holding hand; then BD7DF0 with the owner's 80804D56 reference at offset 30. The engine binds the participant and runs Ghost's animation, Sagira's shell and pedestal VFX. Addresses are resolved anew from the current image and salted references. No recorded process pointer or continuous authority override is included.

The user confirmed Ghost movement and Sagira's shell in the first binding test, then confirmed the final hand-release fix. `build/coo/deadly-trial-revival-20260908/user-validation-hand-release.json` records that acceptance. The last test's clock stopped at 34.292 seconds when the game stopped responding; that test alone does not prove completion. The earlier successful binding capture independently reached 37.762 seconds (authored duration 37.75), inactive, with Ghost retired. The completion guard permits that native frame overshoot and requires a prior authenticated start in the same generation. It cannot complete from an elapsed host timer, missing Ghost, old scene or unbound owner.

The host owns each scan/revival line once. Row 9 must finish before binding is requested. Row 10 waits for actual playback start. Completion joins the native animation end and row 10's submitted voice window, then publishes state 6. The new tests withhold playback after the hold and verify that row 10 cannot start, that both lines submit once, and that stale/unbound/premature completion fails.

This build includes the accepted Pike mount, road triggers, small markers, Walker/barrier behavior, stable Skiff, tower-on-Walker-death activation, tower arrival dialogue, corrected entrance marker and pedestal interaction fixes. Skiff flight-out remains explicitly deferred. The combined fresh playthrough, full completion and precise audio/animation synchronization still need user verification; live acceptance of individual changes is not acceptance of the combined build.


## Revival dialogue ownership correction (8 September 2026 UTC)

The user accepted the integrated animation as perfect, but reported the scan line twice and the later Ikora/Sagira exchange again after Ghost returned. The ced08878 build's log records one host submission each for rows 9 and 10, plus successful native scene start/end and state-6 publication. That proves the duplicates were not repeated host generations; it does not establish single audible playback.

The authored chain `80FBDA45 -> 80F275DB -> 80F275D9` contains **both** native 8080658D dialogue events. In 80F275D9, offset B6D8 names E053D662 (row 9) with start/end 0 seconds. Offset B740 names A9C6FB55 (row 10) with start/end 10 seconds. Both target bank 80F1F086. This was missed when only the old parent selector's audio was considered. Evidence: `build/coo/deadly-trial-dialogue-ownership-20260908/native-dialogue-evidence.json`; independent event bytes: `Dawn/unit/fixtures/deadly_trial_revival_dialogue.bin`.

The replacement removes host `scan`, `scan_finished` and `revival_dialogue` commands. Catalog/document rows 9 and 10 are scene-owned, so the shared queue rejects accidental attempts to enqueue them. The scene begins after the authenticated hold and row 7's submitted voice window. The native summon, hand release, bind, shell/VFX and animation behavior are unchanged.

The native animation lasts 37.75 seconds, while its row-10 exchange begins at 10 seconds and lasts 38.675 seconds. Completion therefore also waits for the native dialogue tail (48.675 seconds from the authored scene origin, plus 250 ms spacing). A source-qualified observer witnesses the current bound owner crossing the 10-second event and retains the remaining voice window, compensating for the observed native elapsed time. This is an authored animation-cue receipt, not a low-level audio-device submission acknowledgement. Missing/stale cues cannot finish the mission, and elapsed wall time cannot substitute for native scene completion. A fresh audible playthrough remains required.


## Accepted full mission and Skiff flight (8 September 2026 UTC)

The user accepted the installed dialogue-ownership build end to end, including single playback of the revival dialogue and mission completion. That acceptance is recorded in `build/coo/validation-deadly-trial-dialogue-20260908/user-validation-e2e.json` and supersedes the pending full-run notes above.

The Skiff departure failed because `8E6F5541` is the authored exit point's name, not an animation action. The recovered native action group is `dropship` (`07EBF354`), with `exit_45` (`7D0D39A9`) as the accepted departure. Plain `enter_45` (`4482A76D`) reaches the original hover point, then carries residual forward velocity about 30 metres beyond it. Extending that command, supplying a waypoint handoff, and shifting the optional action target did not fix the drift.

The accepted entrance is `enter_45_stop` (`052C60A0`). It settles at (938.210632, 407.022644, 154.888260), within 1.4 cm of the original static hover position (938.210754, 407.021027, 154.901825). The user confirmed the complete entrance, hover and exit replay with "FLAWLESS LETS GO" and "Yes, looks right". Evidence is in `build/coo/deadly-trial-skiff-departure-20260908/user-validation-skiff.json` and `live-enter45-stop-sequence-generation-10.json`.

The permanent member authority publishes three native commands: type 9 `dropship/enter_45_stop` at authored arrival point `EB7AF018/58/135`, type 2 wait for 6 seconds while the native Skiff attacks, then type 9 `dropship/exit_45` at point `/136`. Both flight commands use native completion. The parent retains zero loose requests, one member-owned actor, unchanged placement, and the same spawn generation. The member queue revision equals that generation and stays unchanged across ordinary mission publications and Walker death. Native animation, queue advancement and actor retirement own the scene; no process-specific hooks, new host flight timers or position offsets are shipped.

`skiff_authority.h` encodes the 462-bit 80807DA1 body. Its independent fixture was decoded through native `4C74B0` and custom-union decoder `9F7600` in isolated emulation. The decoded 80807F6F queue exactly matches the live-accepted queue after normalizing its test revision. Emulator-only union table initialization bypasses Windows TLS synchronization while retaining the native descriptor mappings; it writes no game memory. Candidate source, tests, DLL and package evidence are in `build/coo/validation-deadly-trial-skiff-20260908/`. The flight itself is accepted live; the compiled packet integration requires the next fresh run.


## Native activity completion and exit repair (8 September 2026 UTC)

The failed ending was not a dialogue timer failure. The completed controller retained its valid completion publication, and an independently captured outgoing type-5 packet contained `4786C0E0/type17/slot3`, state 6/result 1. The actual native lifetime stayed at 3/result 0 and its `ABC090` apply callback never ran. The earlier serializer-only test did not prove native completion.

The retained global group had owner `-1` in the native group registry, and lifetime runtime `80FE12B6 / 80809916 / 718` had sync handle `-1` at `+170`. The live roster still owned the correct lifetime sync record. Restoring those two associations, without writing a completion byte or calling an end function, let the next ordinary packet invoke `ABC090` and change the native body to `06 01` within 201 ms. The user confirmed the mission-complete banner/countdown, then successfully returned to orbit. Earlier exit dumps faulted at `F9C189` after the participation callback resolved a null global activity context. The successful orbit test covers that exit path; character switching has not been separately retested.

`deadly_trial_lifetime.cpp` now repairs only those missing associations at native group-decode boundary `4D7380`, after the receiver's phase-one registration and readiness gate. It requires the current armed/arrived Deadly Trial run, matching native scenario, live roster owner, exact global descriptor, lifetime runtime identity, allocated unique sync record and matching salted handle. It never replaces another valid owner or changes lifetime authority data. Existing scene/audio gates still control when state 6 is serialized. The hook participates in normal call-gate quiescence and protected removal; it includes no recorded process address.

Evidence: `build/coo/deadly-trial-completion-live-20260908/terminal-packet.bin`, `binding-repair-plan.json`, `binding-repair-result.json`, `context-rows.json`, and `diagnostics-restored.json`. All six temporary recording sites were restored before the orbit test. The installed candidate and tests are recorded under `build/coo/validation-deadly-trial-lifetime-20260908/`. A fresh process still needs to confirm the compiled repair installs and fires automatically; the successful test was the equivalent scoped live repair.


## Automatic completion guard correction (8 September 2026 UTC)

The first compiled lifetime repair installed successfully in PID 48136, but never reached its repair writes. Its actor-style self-handle guard required `lifetime + 0x24 == ref.handle`. The actual lifetime service has zero at `+0x24`. This extra check was absent from the successful preceding manual association repair. The initial test fixture copied the same false assumption, so its passing tests did not expose the error.

The guard is removed. Runtime validation still uses native lookup, resolved definition `80FE12B6 / 80809916 / 718`, the current roster and scenario, and the allocated sync record's identity and salted handle. The fixture now preserves the captured zero at `+0x24`; the invalid-runtime case instead supplies a mismatching definition. Both the repair and regression test therefore accept the actual service layout without treating it as an actor.

For the live test, only the six-byte conditional branch rejecting the false self-handle comparison was temporarily bypassed in the installed compiled repair. No owner, sync or completion value was written by the test script. The existing callback logged `lifetime_rebound` at t=760735, restored the two associations itself and let the next ordinary packet change native phase/result from `03 00` to `06 01` in 1.805 seconds. The code bytes were restored immediately. The user confirmed: "Banner appeared; Destiny is closed."

Evidence: `build/coo/deadly-trial-lifetime-auto-20260908/current-binding.json`, `compiled-repair-disassembly.txt`, `compiled-guard-live-test.json`. Corrected compiled candidate, tests and installation records are in `build/coo/validation-deadly-trial-lifetime-auto-20260908/`. This validates the compiled callback on the failed run; a fresh launch with the corrected DLL remains the final automatic-completion rerun. The mission script and accepted Skiff/revival behavior are unchanged.
