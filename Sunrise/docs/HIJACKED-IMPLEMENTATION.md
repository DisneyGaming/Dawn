# Hijacked implementation record

**Current delivery:** the live integration section at the end supersedes the initial implementation and validation status below. Earlier sections preserve the original research and delivery history.

## 1. Mission brief

**Hijacked**, Curse of Osiris, the installed 2017 content: `adventure_rumba`, mission id `hijacked`. Scope is the user's Nessus transcript and walkthrough, from the Artifacts Edge briefing and Sparrow drive through the Tangle/Mists, the Entangled Mind pursuit, cave exit, Well of Echoes platform puzzle, failed processor scan, and the complete Ikora/Sagira exchange. The user requested implementation and DLL installation and authorized one helper agent at high reasoning. The supplied authoring/template documents guide implementation; they do not add unrelated work.

Lua owns eight phases: opening, cave, hunt, mind, surface, well, conflux, ending. Activity-reset/fresh-process replay is supported by new generation ownership. Same-room checkpoint restoration and retail checkpoint selection are **UNKNOWN / NOT YET TESTED**; no checkpoint fidelity is claimed.

## 2. Baseline and evidence

Workspace `C:/Destiny 2 Development`; starting commit `1a84818158c5d3ee07139c2416030df0ec51aeed`. Existing uncommitted edits to `LUA-MISSION-AUTHORING.md`, `MISSION-IMPLEMENTATION-TEMPLATE.md`, and `NEW-MISSION-RECONSTRUCTION-GUIDE.md` were preserved.

The installed starting DLL SHA-256 is `07f8e9c0a730e879b038e1b2eddd0307cc920d07e98ba7be377b250ff968e1cc`, matching [the previous validated package](../../build/coo/deep-storage-door-waves-beam-20260909/package.json). That immutable payload supplies a coherent previous DLL and five-script rollback set. The installer also saves exact pre-install files, which may include newly authored working scripts. The game executable hash is `81964380664e7fcee3c620085a157fdeaf91fefacf7214907820f188bbeb4ced`. Native RVA research used the unpacked image hash `63d128f1c759b92d32b0f226bcbec828bc58cef193ee0df6fdd582bd0290ed1e`; packed and unpacked images are distinct.

**USER REFERENCE:** the full transcript and written walkthrough in the request. [The supplied video](https://www.youtube.com/watch?v=KYQLhatconk) could not be fetched; no continuous playback, sampled-frame review, or timestamp-level visual verification is claimed. **PACKAGE:** generated native identities, polygons, dialogue selectors/durations, tactical rows, resource chains, and the Hydra sequence. Raw evidence is in [hijacked-research](../../build/coo/hijacked-research/README.md), notably `native-bindings.json`, `boss-native.json`, `tactical-joins.json`, `mechanism-resources.json`, and `hydra-teleport-request-disassembly.json`.

**OFFLINE TEST** denotes simulated authenticated receipts, source and wire checks, and compilation. There were no **LIVE READ**, **LIVE INTERVENTION**, or **USER CONFIRMED** gameplay results during this implementation. Native playthrough acceptance remains separate.

## 3. Architecture: reuse before extending

[The mission Lua](../scripts/hijacked.lua) supplies dependencies, population schedules, objectives, dialogue and completion. [The mission directory](../src/state/activity/hijacked/controller.h) contains package bindings and the adapter to the existing executor, mission runtime, lifecycle, object, population, dialogue, objective, and native activity-clock services. Names are registered capabilities; Lua cannot access process pointers or issue arbitrary engine calls.

Two reusable mechanics were extracted from Deep Storage into `coo/scan_playback.h` and `coo/plate_presentation.h`. Deep Storage wrappers preserve its duration and per-plate presentation policies; Hijacked supplies its own source bindings and red unarmed plate pose. Focused Debug/Release Deep Storage tests passed after extraction.

There are **zero new engine detour addresses**. Hijacked extends existing object, combatant admission/death, local position, dialogue submission, scan controller, plate timer and full-body callbacks. The full-body adapter invokes the existing native character-event dispatch using the Hydra's recovered group/sequence. Original forwarding, call gates, bounded component reads and teardown remain at the existing boundaries. It does not set player coordinates or fabricate arrival/death observations.

Every receipt is scoped to a run, generation and exact native source; object/plate/scan receipts additionally bind salted entity/component ownership. Boss movement revisions reject stale acknowledgements. Pointers are freshly resolved within callbacks. A changed identity prevents dispatch; no blind teleport retry is performed.

## 4. Reference reconstruction and native map

**PACKAGE:** activity297, investment `83211FED`, activity tag `80B4200F`, scenario `80B4206A`, root registry `77852DB9`, launch tag `80FB5018`, 45 scenario bubbles. The extractor emits14 registry groups,56 population sources,69 trigger polygons,8 objective rows and14 dialogue rows. All14 groups fit the existing roster capacity; native point-only groups remain admitted for HUD marker references.

The opening profile uses bubble13 / slice104 and native map spawn set `1BD69720`. **ASSUMPTION:** the retail launch selector was not recovered. This spatially selected set contains exactly three points inside the initial Artifacts Edge VO volume, around `(492,50,7)`, preserving the requested drive. The alternative `EF471B4A` spans multiple areas and was rejected. No invented checkpoint key is used.

Mists registry `153E22CD`, bubble33; surface registry `3E9B74F3`, bubble37; Well registry `D997395E`, bubble40. HUD objectives bind native point sources for the Tangle, cave, pursuit, cave exit, Well approach and puzzle, then the actual Mind and conflux source. Marker rendering needs native acceptance.

The same Mind source `153E22CD / type1 / slot21 / 80B421FA` survives the pursuit. Native destinations are `(283.8,307.6,-89.8)`, `(400.3701,359.0769,-96.6716)`, then `(466.3,331.4,-95.4)`; native tactical rows5,2,1 belong to group20. The sequence ABI is independently disassembled: group DWORD0, sequence DWORD6, mode byte at8, coordinates at14/18/1C. **ASSUMPTION:** retreat thresholds67% and34% reconstruct the supplied damage-triggered pursuit; exact retail thresholds were not recovered.

## 5. Triggers, native scenes, and dialogue

Polygon observations begin only in `landing.tv_nessus_m_rumba_artifacts_edge_010_vo`. Earlier crossings are retained after arrival for fast traversal; portal receiving and plate occupancy use current occupancy where required. Player movement releases traversal gates. The original native portal transport must be verified in a fresh run; reaching a receiving trigger in the offline replay does not establish a portal connection.

Dialogue bank `80F5C3A2` submits rows0,4,2,5,6,9,10,11,12,13 once, in that order. These cover the user's complete opening, search, identified Mind, processor acquisition, conflux interaction, failure, and closing exchange. Row6 adds the recovered pursuit reaction. Rows1,3,7,8 have zero recovered duration and are not scheduled.

Native submission establishes the beginning of a voice window. The final row13 lasts28080ms; mission success waits for that full window, after the7141ms failure row12. The8s native scan runs separately from the2500ms hope line. There is no arbitrary timer that pretends the scan succeeded. Native audible playback and subtitle fidelity remain unverified.

## 6. Devices, plates, destructibles, and clocks

The Mists barrier object `80B42351`, slot28, and device `80B4235C`, slot29, use graph `80F567A6`. The exact-device logical mode maps closed to native0 and removed to native1, using the recovered graph already proven for Deep Storage. Only a genuine Mind death releases it; final-arena guards may survive.

The Well red plate is source `D997395E / 80B429A9 / type4 / slot19`, volume `80B4238D / type60 / slot114`. Timer `815B8B3B / 80804FCB / 248` and device `80C7063B / 80803910 / A78` are resolved under the same spawned entity. **ASSUMPTION:**5s charge time. Unarmed pose is native.2; charge/completed pose is.1. Current player occupancy, no enemy contest, an actual charging sample and native timer completion in the same revision are required. Departure/contest before completion invalidates progress. Completed charge stays latched when the player leaves. Presentation reconciles both actual and pending target values under fresh ownership.

Charge creates the eight block/cover sources and activates the recovered block/sniper devices. Block1→2→3→4 crossings lead to the final guards. Source placement and resource identity are **PACKAGE**; materialization appearance, collision, device direction and complete jumpability are **NOT YET TESTED**.

Conflux source `D997395E / 80B423B5 / type4 / slot23` uses entity `80F4B586`, controller `8157E6B1 / 80804D3A / 358`, linked to `40A009B5 / 80B42410 / type65 / slot0`. The native instance duration override is8s, replacing the3s base; the shared predicate checks effective duration, overshoot, participant, started state and generation. Completion without a prior valid native start is rejected. A monotonic native activity clock supplies gameplay authority; no wall-clock value is substituted for native timer completion.

The ending projection source `80B429C6`, slot24, uses device `80B429CC`, slot25, and graph `80F4B5A5`. Its recovered.45–.55 branch selects the.5 presentation. The source survives the entire closing exchange and completion. Visible hologram/image fidelity is **NOT YET TESTED**. No destructible lens or unrelated Deep Storage mechanic is added.

## 7. Infinite Forest and generated routes

Not applicable: Hijacked remains on Nessus. There is no Forest generator, cross-destination transition, Mercury handoff or recreated Infinite Forest pass in this mission.

## 8. Encounters, environmental performances, and ending

Surface patrol clearance, cave entry/small-arena clearance and the pursuit's front/back cohorts follow the user's walkthrough. Requests allow streaming to admit actors later; clears require admitted native actors and genuine death receipts. Surface/Well traversal populations and the Mind's final guards do not all become mission-wide death requirements. The final conflux defenders are cleared before inspection. Normal and early boss-death routes are tested; early death releases movement waits without synthesizing relocation.

**PACKAGE:** source definitions, native spawn rules/categories, health components and tactical assignments. **ASSUMPTION:** one actor per recovered source/category; exact retail wave multiplicities and some encounter timing are not independently established.55 native source placements were joined. Cave source7 has a recovered rule but no placement in mission-only data; it uses a documented native tactical-row fallback. Health/AI/tactical readiness remains a separate native observation, not an automatic consequence of source creation.

No separate Mind Core pickup source was found in the recovered activity registries. The script plays the processor-acquisition exchange after genuine Mind death; it does **not** implement or claim a separately authenticated manual pickup or inventory grant. Native death reward/drop behavior is an explicit fidelity gap for the first playthrough.

The final scan enables the failed-processor image and dialogue12, then the complete Ikora/Sagira past-code exchange. Success clears the objective and respawn restriction after row13 finishes. Unrelated surviving actors and the required ending projection are preserved. Reset retires the old generation and rejects old receipts.

## 9. Live experiment record and persistence

No live memory experiments were performed. No game process was launched, debug forcing used, or scene replay presented as acceptance. The DLL-relative Lua definition is immutable after loading; changing it requires a fresh process. No launch/settings changes are required for installation.

A shell sandbox setup problem required approved elevated execution of local development commands. Automatic review rejected one proposed baseline restoration due to possible uncommitted-work loss; that command was not executed. Development continued by preserving current contents and making integration edits directly. The user's three pre-existing guide edits were not overwritten.

## 10. Tests, package, install, and acceptance

New suites `hijacked_tests` and `hijacked_catalog_tests` are registered alongside20 previous suites:22 suites in Debug and Release plus both DLL builds =46 results. Packaging includes six mission scripts, DLL, PDB and Lua license. The installer enforces46 results and matching source/payload/baseline hashes, refuses a running Destiny process, backs up exact prior files, verifies installed hashes and restores the prior files on copy failure.

Focused `hijacked-replay-dev1` passed the initial Debug route. `hijacked-shared-dev1` passed Hijacked and Deep Storage route/catalog checks in both configurations (6 results). The replay covers authentic admission, wrong owners, duplicate admissions, boss health, relocation revisions, early death, barrier hold, unarmed plate behavior, departure/reentry and enemy-contest charge interruption, retained completed charge, scan start/finish, optional survivors, full ending timing and reset. `hijacked-catalog-dev1` passed both configurations (2 results). Catalog checks use independent package constants and wire decoding, including nonzero generation/device values and undersized writers. Offline simulation does not prove engine playback.

The first full candidate, `hijacked-20260909-final1`, was stopped after linker errors exposed missing Hijacked adapter sources in older shared-wire test projects. Those projects now link the adapter with a unique object filename. The failed receipts remain preserved; no check was bypassed.

The second full candidate, `hijacked-20260909-final2`, exposed an isolated position-test callback stub dependency. The harness now verifies Hijacked receives the same authenticated coordinates and publication count as the other mission adapters. That failed candidate was also preserved without packaging or installation.

The third full candidate, `hijacked-20260909-final3`, stopped on disk exhaustion. Only this task's intermediate `obj` directories were removed, preserving outputs and receipts. Its replacement uses compressed build storage without changing file bytes or weakening validation.

The fourth candidate, `hijacked-20260909-final4`, passed all44 test results but both DLL builds exhausted disk space. Its failed logs and source archive are preserved. Intermediate directories of completed historical builds were then compressed losslessly, deleting no historical files and verifying the previous payload hashes.

The frozen final candidate is [hijacked-20260909-final5](../../build/coo/hijacked-20260909-final5/acceptance.md). Its `results.json`, `failures.json`, `source-manifest.json`, `package.json`, `installation.json` and post-install verification are the authoritative final status. This source document is frozen before full validation; those companion receipts record results afterward without changing the archived source. Superseded/failed development outputs remain distinct.

Fresh-process acceptance is **NOT YET TESTED**. Required checks: opening control and marker; patrol AI; three native boss poses with `teleport_requested`/`arrived` logs; real death/barrier/core behavior; both portal connections; unarmed red plate and interrupted charge; materialized platform collisions and final landing; actual8s Ghost scan; projection and every audible closing line; successful full unforced run and retry behavior.

## 11. Delivery record

Launch through the usual `launch-destiny.cmd`, select **Activity override → Hijacked opening**, then use the normal Chosen/activity282 donor route. The native prelaunch profile selects Hijacked/activity297. Logs under `Sunrise/logs/sunrise.log` should identify mission `hijacked`, `format=lua`, the DLL-relative script path/fingerprint, executor sections and native receipt stalls.

The final candidate carries matching source/payload archives, symbols, validation results, installation hashes and rollback evidence. Refer to its [acceptance record](../../build/coo/hijacked-20260909-final5/acceptance.md) for actual installation status. No fresh native full-run acceptance is claimed. The first focused native check is correct opening/AI and the first Mind relocation; preserve the requirement to complete the entire unmodified route afterward.

## Loading-card identity correction — 9 September 2026

The first native launch exposed a defect in the original profile: activity296 / investment3BBB85F8 belongs to `mission_pact` (Tree of Probabilities), while the destination override still loaded `adventure_rumba` on Nessus. The user's screenshot and the preserved log establish the mismatch: service6 first selected `mission_pact` at activity296, then forced `adventure_rumba`; the Hijacked Lua was loaded and its opening executor started. This is launch identity failure, not proof of a complete mission playthrough.

The corrected identity is activity297 / investment83211FED. It is derived by joining package77852DB9 in definition table81327CD8 to the activity ordinal, then joining that ordinal to public table81327CF0. The activity resource80B4200F independently names package77852DB9 and scenario80B4206A. The original extractor only asserted a known public row/hash and therefore failed to verify that it belonged to Hijacked. It now requires the package join, and the catalog suite checks the production prelaunch profile against the correct identity and rejects296.

Correction evidence and the pre-fix log: `build/coo/hijacked-launch-identity-research/`. The correction candidate is `build/coo/hijacked-launch-identity-fix-20260909/`; its acceptance and installation receipts supersede the original launch identity while preserving the previous candidate archive. Correct loading-card presentation requires a fresh game process after replacement; full mission gameplay remains unverified.


## Opening publication and Baboon repair — 9 September 2026

The user's next run reported no objective or dialogue, followed by Baboon. The preserved `build/coo/hijacked-baboon-research/sunrise-before-fix.log` shows primary activity001 joined at77641ms, auxiliary group activity002 joined at92516ms, successful type5 mission updates through104688ms, and Hijacked's first active frame at105688ms. No type2 clock notification or subsequent type5 update was emitted. At125203ms the native primary activity watchdog reported20518ms without a host update. A requested objective in executor state is not evidence of native display.

The clock publication guard compared the primary activity with `newest_joined_activity()`, which includes auxiliary hosts. Once the mission activated its clock, the newer auxiliary activity caused the primary's whole atomic notification bundle to be discarded. Clock ownership now checks the bound activity's exact creator lineage, retaining the current binding, nonforeign, arrived, mission-run and generation checks. The shared clock path serves Hijacked, Deep Storage and Beyond Infinity. It does not extend the watchdog timeout or bypass transactional delivery.

The complete startup roster regression reproduces the live2736-byte inactive packet and5868-byte active objective/dialogue packet; it also checks all object envelopes and atomic rejection of a short buffer or invalid dialogue generation. A regression checks primary ownership with a distinct newer auxiliary host, rejects borrowed and mismatched lineages, and rejects a reused session ID with a different incarnation. A bounded publication diagnostic distinguishes a staged frame from a blocked clock, sensor encoding or notification; staged remains distinct from native playback. Candidate validation and installation receipts live in `build/coo/hijacked-host-ownership-fix-20260909/`. Fresh native opening and full mission acceptance remain required.


## Live integration and native placement entities - 9 September 2026

The user completed a guided native playthrough with live interventions. The original run exposed missing upper-ledge and Mists populations, pursuit progression stalls, missing objective/dialogue publication, early conflux invisibility, and platforms appearing together. The real final conflux interaction and ending completed: native dialogue rows12 and13 submitted, then the controller reported `mission_finished=1`. Evidence is in `build/coo/hijacked-mists-spawn-research/LIVE-ACTIVATION-20260909.md` and `live-mission-completion-evidence.log`. Manual progression during that run is not acceptance of an unforced route. The game exited after completion; this integration does not launch it again.

The placement lookup is now part of the DLL. `generate_hijacked_placements.py` joins all56 enemy source definitions to63 authored placement bindings, including the previously missing Mists source7 in shared table81582ABD. Native tables80B420E0,80B42549 and80B427F3 cover the cave, surface and Well respectively. Each binding retains its table, row, template, GUID and full descriptor hash. `hijacked_placements.cpp` creates a missing placement through the existing native575690 constructor trampoline, using its original loaded table descriptor and native table ownership. It requires the correct mission and native context, freshly resolves the selected native registry/source with matching self references and the current authority generation, verifies the descriptor, and checks the resulting salted object identity and actual pose provider. Existing placements are reused. No substitute template, invented transform, context override or fabricated enemy admission is used.

Enemy sources publish their current native generation with zero requested population until their placement entities pass preparation. Only then does the existing combatant authority request the source's configured categories. Real actor admission, AI readiness and death remain separate requirements. This prevents a one-shot spawn request from being consumed while its placement entity is absent. Placement readiness checks the authority generation that native4E8FB0 applies even with zero requests; actor-result sense generation remains an enemy admission check and cannot delay initial placement creation. Required population overflow remains a fault; optional traversal survivors do not block mission completion. Native placement diagnostics distinguish context, descriptor, creation and provider stalls.

The Lua now separates the upper ledge patrol from the full lower patrol, and schedules the first Mists Harpies before the follow-up cohort. Initial boss movement is requested without an unnecessary initial-arrival prerequisite; later retreats still require real movement acknowledgements. The conflux is requested from the surface phase and remains visible through the ending. Charging creates the first platform; current occupancy on each landing area creates the next platform through block05. Platform-specific guards wait for the corresponding geometry. Floor defenders are requested before ascent and must be cleared before inspection. Actual scan completion and final dialogue still control success.

Native presentation adapters now reconcile the requested Hijacked objective and current dialogue row through existing native callbacks. They retain exact resource, component, run and generation ownership, and revalidate after native calls. A foreign auxiliary session no longer clears the owned Hijacked controller. These changes persist the live objective/dialogue and mission-state repairs in normal execution.

Per the user's explicit scope, this candidate uses only Release `hijacked_tests`, Release `hijacked_catalog_tests` and the Release Sunrise DLL build. The focused `hijacked-release` package profile requires exactly those three successful results plus the normal source, payload, binary and install-integrity checks. No Debug build, game launch or unrelated regression suite is part of this delivery. The candidate directory is `build/coo/hijacked-live-integration-20260909`; its validation, package and installation receipts record the actual results after the source is frozen.

The current source is implemented and checked offline; a fresh native run is still needed to verify all newly created placement entities, full wave appearance, movement, collisions and sequential platform behavior together. Exact retail population multiplicities beyond recovered native source/category data and a separate manual Mind Core pickup remain unverified. This is a limit on gameplay acceptance, not deferred placement integration.


## Placement startup correction - 9 September 2026

The first playthrough of the placement integration exposed a production wiring error. Read-only capture of PID51472 with the installed c1940b81 DLL proved the Hijacked controller was enabled, started and arrived, with no population fault, but the placement adapter's gate, image, constructor and helper pointers were all zero. All four native helper signatures matched. Its only startup call was inside the retired activity-spawner probe, which production permanently disables. Enemy requests consequently remained held pending a preparation callback that could never run. This is confirmed startup failure, not a missed ledge trigger or signature mismatch.

The placement adapter now owns a narrow verified575690 detour installed directly in the normal bootflow lifecycle before world-step polling. Its installation result participates in both lifecycle success summaries and emits explicit startup diagnostics. Original forwarding waits for trampoline publication during the Detours commit window, remains intact after quiescence, and protected teardown retains all state if removal is deferred. The retired probe's placement startup and teardown dependencies were removed; that probe stays disabled. The read-only evidence is in `build/coo/hijacked-placement-live-20260909`.

This correction candidate is `build/coo/hijacked-placement-startup-fix-20260909`. It uses the same two Hijacked Release suites and the Release DLL build only. The failed run's process exited during diagnosis; no live writes, fabricated readiness or game relaunch were performed. Fresh native acceptance of the corrected startup and subsequent placement creation remains separate from compilation.


## Final encounter correction - 9 September 2026

This section supersedes the initial reconstruction's one-per-category density,
small-room death gates, rounded boss thresholds, and extra sniper slab.

The native source encoder previously requested only one actor in each category.
The per-source policy now explicitly requests three for infantry/composite
groups, one elite plus two escorts for mixed groups, and one for individually
placed Harpies, snipers, elites and singleton boss/member producers. The receipt
ledger expects exactly those totals. The mapping, evidence and remaining retail
count uncertainty are in [HIJACKED-SQUAD-COUNTS.md](HIJACKED-SQUAD-COUNTS.md).
The actor counts are a user-directed reconstruction; package template choices
and spawn-effect counts are not treated as proof of retail multiplicity.

The Mists entrance and small-room cohorts do not require kills. Small-room
entry requests Harpies and infantry independently, and genuine large-room entry
releases the boss phase even if all 19 earlier actors survive. These cohorts
are nonblocking for excess admissions too. The surface patrol's local clear
gate remains, with its previously excluded remote Harpy preserved. Hunt guard
clearance and final conflux defender requirements remain explicit in Lua.

Sagira's identification line queues two seconds after the first genuine
large-room/boss-seen volume entry. The runtime timestamps the native position
observation; an owned EventTimeline retains it through phase changes and
rejects reset/run reuse. It is independent of initial boss teleport/readiness.
Actual audible start still follows the serialized native dialogue queue.

The Entangled Mind's body damage is limited to exact remaining fractions 2/3
and 1/3. The existing B804E0 damage callback adjusts only the authenticated body
entry's target health fraction; even a lethal hit cannot cross the current
floor. CDCB60 reports the body as immune at the floor and while relocation is
pending; actual arrival with the matching actor, phase and revision unlocks the
next third. Final-phase lethal damage uses the original native path. Identity
checks include the current mission run/generation, salted source and actor,
character health reference, exact Hydra health and body-region definitions,
native region-index bias and pinned accessor prefix. No encrypted health word
is overwritten and no extra damage detour is installed. The disassembly
evidence is in build/coo/hijacked-e2e-research-20260909.

The unused Well sniper support (D997395E, object10/device9) is never requested.
The five sequential route platforms and their covers remain, and the conflux
stays visible throughout the climb, interaction and ending. Each subsequent
platform still needs current occupancy of the previous authored landing area.

The prior live log's mission_finished=1 proved only controller completion.
The native global lifetime could retain stale owner/sync associations after a
region retirement, and the existing native decode-boundary repair was scoped
only to A Deadly Trial. It now also accepts the exact current Hijacked scenario
80B4206A and repairs only the retained owner/sync links before native decoding.
The native lifetime body is untouched; real host completion authority must
apply state6/success. Hijacked also keeps publishing terminal authority at its
existing cadence until its run retires, instead of stopping after one packet.
This addresses a code-supported failure path; visible fresh-run completion is
not claimed from an offline controller flag.

The candidate is build/coo/hijacked-e2e-final-20260909. Validation is limited to
Release Hijacked replay/native contract tests, Release Hijacked catalog/wire
tests and the Release Sunrise DLL. This includes optional-room traversal,
1999/2000ms dialogue timing, authentic teleport unlocks, large-hit body floors,
foreign/stale native identities, exact squad totals, sequential platforms,
absence of the sniper slab, scan/closing dialogue completion and lifetime
binding repair with body preservation. No Debug or unrelated suite, game
launch, live state forcing or fabricated receipt is part of this correction.
Build/package/install results are recorded in that candidate directory. A
fresh game process loads its matching DLL and Lua; visible ending, immunity
feedback and full group placement still require native playthrough acceptance.


## Final route and teleport arrival correction - 9 September 2026

This correction supersedes the earlier five-platform route and player-volume
reinforcement gates. The remaining bright floating slab is Well object18,
`o_echoes_rumba_block05`, source80B429A6 / GUIDF04DCAD5B76D53AC. Its authored
model, position beside and above the conflux, and the live creation timestamps
match the user's screenshot. It is a different object from the already omitted
sniper support. The Lua never requests block05. Block04, its device and covers,
and conflux object21/device22/scan23 remain. The final landing completes the
climb without creating another platform. The evidence and limits of the object
identification are preserved in
build/coo/hijacked-final-route-fix-20260909/slab-identity-handoff.json.

The five second-room back/exit sources now request immediately after the
owned phase1 boss arrival. The four final-room sources request immediately
after the owned phase2 arrival, while the player may still be crossing the
second room. Neither request depends on a player chase/final-room volume or
on earlier guard deaths. The final-room volume still advances the objective;
existing front/back clear requirements remain separate. Native placement
preparation, actor creation and readiness still determine actual appearance.

The last live run exposed arrival checks against authored points, even though
native10D0020 projects the destination and adds the Hydra height offset before
10C7273 copies selector+B0 into the native motion request. The final captured
endpoint (472.2383,328.2527,-94.9722) differs by over six metres from the authored
point. Phase1 acknowledgement took over three minutes; phase2 required the
user's live release. The permanent observer now checks the actual world pose
within one metre of the retained native resolved endpoint after sequence6 is
inactive. It requires an issued current actor/phase/revision lease plus observed
activation or a changed native endpoint. Component identities and the endpoint
are revalidated after the native world-pose getter. A later replacement endpoint
or sequence invalidates the retained evidence. This is verified arrival at the
native resolved destination, not an invented animation-completion receipt.

A separate dispatch reservation is released on all pre-call validation aborts;
only the validated call boundary marks the command issued. Actual native
teleports are not blindly retried. Native calls execute outside the lease lock.
The exact third health floors and relocation immunity remain; authenticated
arrival unlocks the next third and requests its reinforcement wave.

The candidate is build/coo/hijacked-teleport-wave-fix-20260909. Validation is
limited to Release hijacked_tests, Release hijacked_catalog_tests and the
Release Sunrise DLL. The focused cases include adjusted native endpoints,
active/stale/replaced selector rejection, phase1/phase2 reinforcement requests
without player movement or earlier kills, and block05 absence through the final
landing and ending while the conflux support remains. Its build/package/install
receipts record actual outcomes after the source is frozen. No game launch or
live state forcing is part of this delivery; fresh native acceptance of this
candidate remains separate from offline checks.


## Loose arrival allowance and delayed identification - 9 September 2026

This correction supersedes the preceding endpoint-latching requirement and
2-second identification delay. Live PID62696, run2/generation129, proved that
the native phase1 endpoint changed by about0.385m and phase2 by about6.7m after
its first observation. Both selector sequences were inactive6, both live leases
were marked invalid, and both health floors remained locked until the user's
manual release. Evidence is preserved in
build/coo/hijacked-boss-gate-live-62696 and
build/coo/hijacked-boss-final-gate-live-62696. Those releases were explicit manual
checkpoints, not native arrival acknowledgements.

Per the user's request for very loose checks, arrival now allows a15m radius
around the latest native endpoint. It never freezes a provisional endpoint or
permanently invalidates a request when that endpoint changes. The same predicate
is checked again with the freshly read selector after the native world-pose
getter. Issued current actor/phase/revision ownership, inactive sequence6, finite
coordinates, and native activation or a changed endpoint still apply. This
intentional spatial allowance can acknowledge arrival while the Hydra is still
several metres from the endpoint after teleport activity stops. It is not an
exact animation-completion receipt. The already-positioned pre-dispatch check
retains its existing1m radius. No new hook is added.

Sagira's identification line now queues10seconds after either real large-room
or boss-seen entry. Both named Lua capabilities are after10s with10000ms event
observations. Native dialogue serialization still determines audible start.
Arrival-triggered reinforcements and the removed block05 remain unchanged.

Candidate: build/coo/hijacked-loose-arrival-20260909. Only the two Hijacked
Release suites and the Release Sunrise DLL are built. Focused coverage includes
both captured provisional-to-adjusted endpoints,14m acceptance/16m rejection,
active/wrong-sequence rejection without permanent poisoning,9999/10000ms VO
timing, and the existing Hijacked route. Package and installation outcomes are
recorded in the candidate directory after source freeze. Fresh native acceptance
of this candidate remains pending; no game launch is part of this delivery.


## Patrol preload, nonblocking chase and Well escorts - 10 September 2026

This section supersedes the earlier exterior Harpy trigger, return-route spawn
triggers, chase clear waits and plate-charge Harpy schedule. The user clarified
that the exterior Mists Harpies should be waiting from mission load, return-route
standing Vex should request when the Entangled Mind dies, and the Harpies to halve
are the final Well escorts.

Opening requests surface sources1/2/6 immediately with the briefing; approach
and drop still release the existing infantry. On actual Mind death, the mind
graph requests surface Vex sources9–17 before the cave exit and return portal.
Their later surface-trigger spawn steps are removed. Dormant Fallen sources
18/20/21 remain disabled. Early requests still pass the existing native placement
and streaming gates; neither stationary appearance nor omission of a native
creation effect is fabricated. No patrol-versus-teleport-effect flag was recovered.

The hunt graph has no front/back enemy-clear waits. Following the second damage
gate and authentic final teleport arrival, third-room entry requests objective3
and the four final Mists defenders. The existing pursuit line6 also follows that
entry and identification queueing, preserving voice order and the10second
identification delay. Second-room enemies may all survive. Only real Mind death
releases its barrier and the processor exchange.

Well Harpies no longer request on plate charge. At the final encounter, the
Hydra (native name `sq_final_minotaur`, source35) and Harpy sources29/30 request
together. Each Harpy source retains its native three-actor request;31/32 remain
unrequested, reducing the escort from12to6. Spawn locations, source definitions,
health gates, the15m arrival allowance, and the removed spare platform are
unchanged. See HIJACKED-SQUAD-COUNTS.md for the misleading native Hydra name.

Candidate: build/coo/hijacked-patrol-timing-20260910. Validation remains limited
to Release hijacked_tests, Release hijacked_catalog_tests and the Release DLL.
Focused cases exercise exterior Harpies before approach, return patrol requests
before the exit portal following actual death, all16 second-room guards alive
through objective/VO/final-defender progression, and six Well Harpies alongside
the final Hydra rather than on charge. Candidate receipts record actual build,
package and install outcomes. Fresh native acceptance remains pending; no game
launch is part of this delivery.


## Immediate retreat dispatch and staged return reinforcements - 10 September 2026

This correction removes authority-publication cadence from health-gate retreat
scheduling. After the original native B804E0 damage callback returns, the existing
Hijacked damage adapter reads the authenticated applied health fraction. A real
floor crossing runs the existing Lua graph immediately through health_event,
then dispatches the requested native retreat from the same callback. It does not
publish a predicted fraction from the damage packet. The full-body observer also
refreshes the same actor/owner request after its health observation so it can
issue a newly requested retreat in that callback instead of waiting another tick.

The immediate dispatcher holds the existing cinematic CallGate, copies only a
previously authenticated character handle, freshly resolves its animation/biped/
full-body chain, and enters the existing complete boss validation and dispatch
reservation path. Runtime and lease locks are released before engine calls.
The wrappers and helpers are included in both existing owners' protected teardown
lists. No new detour address, forced position write, synthetic native arrival,
health-floor change or timer-based immunity release is introduced. The native
animation system still controls visual playback after request dispatch; an
exact zero-millisecond rendered transition is not an offline acceptance claim.

Return-route scheduling now separates patrols from reinforcements. Surface9/13/14
remain requested on actual Mind death. Their simple80BFDDC8 point producers use
native80806723/80806725 components. Surface11/12 request on echoes.entry;
surface10/15/16/17 request on echoes_fallback01_trigger_volume. These delayed
sources use composite80C4BB85/80C762C2 producers; the latter includes additional
pose/state-machine resources. This is an explicit reconstruction supported by
producer structure, not a recovered retail patrol/reinforcement flag. Original
placements and native effects remain intact. Zone entry is the chosen engagement
trigger; no new damage-based proximity detector is required.

Candidate: build/coo/hijacked-immediate-retreat-20260910. Only Release
hijacked_tests, Release hijacked_catalog_tests and the Release Sunrise DLL are
validated. The focused replay checks both gate requests before any separate
publication update, foreign/duplicate health rejection, patrols on death, no
reinforcements at death or return portal, and independent entry/combat-zone
reinforcement activation. The previous Well escort reduction, nonblocking chase,
10second identification delay and15m arrival allowance remain. Candidate receipts
record actual build/package/install results; fresh native playback acceptance is
pending. No game launch is part of this delivery.


## Tunnel progression and smaller Mists reinforcements - 10 September 2026

The opening objective now updates at the authored tunnel-mouth music volume:
F5737F85/type60/slot15 (tv_entering_the_lair_music), before Mists intro136.
The native tunnel endpoint5 and later intro136 are downstream fallbacks.
The patrol-death gate is removed. Retained tunnel entry also releases missed
approach/drop observations, so running through cannot strand the opening phase.
Cave entrance source requests still occur at intro136; only objective timing
moves earlier. Authored volume polygons are unchanged.

Mists composite reinforcement sources7/8/9/10/12/19/23/24/25 now request two
actors each. Standing simple-point sources2/14/15 retain three, as do other
areas' existing grouped sources. Single enemies and native placements are
unchanged. Producer families are recovered package evidence; classification
and density are explicit reconstruction choices requested by the user.

Candidate: build/coo/hijacked-tunnel-two-vex-20260910. Verification is limited
to Release hijacked_tests, hijacked_catalog_tests and Sunrise DLL. Focused
fixtures check the objective before load-zone entry, progression with all
opening enemies alive, missed approach fallback, exact count wire values,
and real per-source admissions/deaths. Fresh native playthrough remains pending.


## Dialogue cue alignment - 10 September 2026

The tunnel objective1 and patrol dialogue2 now request in the same Lua parallel
step at tunnel entry. Cave-load and entrance-source dependencies no longer delay
the line. Pursuit dialogue6 requests on the authenticated second teleport arrival
(final_position), without waiting for third-room entry or the identification cue.
Existing native dialogue serialization remains: cues do not interrupt a line
already playing. Enemy, objective3 and room-entry scheduling otherwise remain.

Candidate: build/coo/hijacked-dialogue-cues-20260910. Only the two Hijacked
Release suites and Release Sunrise build are required. Focused replay checks
same-update objective/dialogue requests and pursuit requested at second arrival,
then submitted before room3 entry. Native playback acceptance remains pending.


## Retire bypassed exterior Vex at Mists load zone - 10 September 2026

At Mists intro136 the Lua graph permanently retires surface sources1 through7,
including surviving outside patrols, after their initial requests. The earlier
tunnel objective/VO and cave population schedule are unchanged. Source retirement
remains requested across later phases and backtracking; it is not an enemy death.
The controller filters retired receipts from living/readiness consumers and
rejects late admissions, readiness and deaths without inventing clearance.

PACKAGE/native-code evidence:4E4580 queues positive cumulative spawn deficits;
zero counts alone do not remove old actors. Native4E9550 compares incoming
80807EC9 authority+7C generation with source+244. A changed generation with
BC=0 calls4EC1A0, enumerates source-owned actors and invokes native56A8F0 entity
lifecycle removal. BC=1 instead uses4EC410. Reflection offsets124/184/188/189
map to generation fields and signed retirement mode: wire1 at bits603..604
means BC=0; normal loose wire2 means BC=1. Research is preserved under
build/coo/hijacked-retirement-research-20260910/native-generation-decomp.txt,
with reflection in build/coo/validation-gateway-traversal/native-wire-evidence.json.

Retired sources advance generation once, publish zero category counts and
BC=0, retaining the new generation. The shared writer's new retireOwned option
defaults false and preserves all existing callers' normal bytes. Only Hijacked's
seven exterior source aliases expose this retirement operation. No new hook,
manual entity write, synthetic kill or direct engine-function call is introduced.

Candidate: build/coo/hijacked-exterior-retirement-20260910. Validation is limited
to Release Hijacked gameplay/catalog suites and Release Sunrise. Tests cover
outside survival until load entry, retirement and new generation at load entry,
late callback rejection, no reactivation on backtracking, and exact native wire
removal mode/generations with zero requests. Fresh native playthrough is pending.


## Retirement diagnostics after failed live removal - 10 September 2026

The25687B77 run loaded the matching Lua and advanced through retirement into
section1 at143719ms, but lacked native removal readback. This proved the request,
not entity disappearance. That log is preserved under
build/coo/hijacked-retirement-failure-20260910. Further audit confirmed that
4EC1A0 uses iterator filter1 (actor+60 == -1); its scope is narrower than all
source-owned actors. Whether the surviving actors match that filter is unknown.

The diagnostic candidate logs ev=hijacked_retirement stage=request once per
source. Existing placement game-thread polling adds read-only native_readback
at most every250ms on changes, with5s unchanged summaries. It captures desired
versus source authority/sense generations(+1FC/+244), request revision(+238),
removal policy(+23C), initialized flag(+260), owned list count(+2F0), pending
request/count(+310/+314), context bubble and general native authority availability.
The latter is not a direct invocation or result of4E8060.

Actor readback merges retained authentic receipts with a bounded native source
list(up to64). It logs salted actor identity, actual/expected source owner,
actor+60 filter field, actor flags and identity-checked entity removal flag.
status0=unreadable;1=actor salt changed;2=actor found/entity unreadable;
3=entity salt changed;4=actor and entity identity matched. UINT32_MAX and
INT32_MIN sentinels mean unavailable. entity_removed=1 requires status4 and
entity+4 bit2; missing or reused records are not called successful removals.
Source reads are revalidated and run ownership is checked before output.
Diagnostic generation observation is separate from creation's strict generation
check. No native mutations, new hooks or gameplay scheduling changes are added.

Candidate: build/coo/hijacked-retirement-logging-20260910. Only Release Hijacked
suites and Sunrise build are used. The existing retirement fixture also verifies
that diagnostic identities survive logical retirement. Fresh native removal
readback will come from the next user run; installation alone proves no fix.


## Explicit exterior entity teardown after live retirement failure - 10 September 2026

LIVE READ (PID58816, logging candidate): retirement was requested/applied for all
seven sources, but 14 matching Vex entities remained with detached actor owners
and unset local object-authority bits. Old ledger salts had expired. The initial
source-filtered scan missed these entities; it did not establish successful
removal. Post-event bits do not prove the exact pre-retirement authority state.

Native audit establishes that4EC1A0 gates56A8F0 on local object authority, then
A93E40 detaches actors regardless. Correct source owned count is+314; weak entries
start+318 with the actor handle at+31C. The earlier diagnostic+2F0 count and+314
pending_count labels were incorrect; the current diagnostics correct those reads
and include each matching entity's local object-authority bit.

The fix retains authentic exterior source1..7 actor/entity/bundle identities
at the existing A0D510 admission boundary and from native weak lists on every
game-thread poll while actors are still linked, including actors recreated by
streaming that the gameplay ledger cannot admit. Unready admissions defer to
the poll. The cache is keyed by entity slot, so reusing an actor slot cannot
overwrite another surviving entity lease. At Lua retirement,
the top-level placement game-thread poll calls native56B550 on matching targets.
Constructor callbacks only retain identities; they cannot delete an entity while
a native caller is still consuming the constructor result.
This is the same entity teardown used by source destruction4E3C50 viaA84480; it
uses the entity's stored scene membership and global object world, not the
player's current bubble. No new detour, authority bitmap edit, simulated kill,
mission advancement or spawn-anchor deletion is involved.

Every dispatch requires the current mission lease and retired source, unchanged
entity table/stride/namespace/world-manager identity, exact entity salt and
bundle. Capture requires the full actor/source identity and independent AI-parent
backlink; actor or AI-parent expiry afterward does not invalidate the surviving
entity. The current actor table must not link that entity to another owner. A
previously authenticated actor may have a cleared owner. Unknown detached actors
cannot create a deletion lease. Calls occur once per retained identity; logs
separate entity_retained, entity_identity_expired and entity_delete_returned
(result0 unreadable,1 retired/replaced,2 still present). Native return alone is
not called success. The existing zero-request retirement prevents respawning.

Candidate: build/coo/hijacked-exterior-delete-20260910. Validation remains Release
hijacked_tests, hijacked_catalog_tests and Sunrise DLL only. The focused catalog
fixture reproduces detachment and actor reuse with a surviving entity, and
rejects entity reuse, reassignment, changed bundle/world and unauthenticated targets. Fresh game acceptance
is required; the previous process was already closed before this patch install.


## Retire before exterior streaming - 10 September 2026

LIVE READ: the ccbe887c candidate captured fifteen entities in run2 of PID44812,
then discarded fourteen remaining leases at Mists intro136 and made no explicit
native deletion calls. Original entity slots were later proven reused; thirteen
detached Vex were present after returning outside. Source instances were absent
in Mists and reconstructed with new handles on return. This does not establish
whether each first expiry was a world mismatch or an already-recycled entity.
Evidence: build/coo/hijacked-exterior-delete-live-44812/findings.json.

The user authorized the revised timing. Lua now retires surface sources1..7 at
the authored tunnel-mouth condition, before Mists streaming unloads the exterior.
Their initial requests still precede retirement; cave entrance requests still
wait for intro136. Objective and dialogue timing remains at the tunnel mouth.
Retired sources stay disabled across backtracking and late callbacks. Deleting
the original population while it is owned is the primary prevention against
restoring surviving detached copies after the unload.

A0D510 often supplies a valid actor and AI-parent before its entity exists. The
adapter now retains that authenticated source/actor/parent origin and binds the
later ready entity on the existing game-thread poll, even if source ownership
was cleared in between. Actor/parent salts and the actor-table instance must
still match; this does not invent lineage for a completely recreated detached
actor. Constructor callbacks still cannot perform deletion. No new hook is added.

World/context mismatch and unreadable memory now defer retained entities. Only
a readable matching-world result proving replaced salt/bundle or removal expires
a lease. Logs distinguish unreadable0, otherWorld1, replaced2, removed3 and live4,
including expected/actual entity table, stride, namespace and opaque world token.
The raw2744A18 value is encoded; it is compared as an identity token, not used as
a pointer. Native deletion and original ownership checks otherwise remain intact.

Candidate: build/coo/hijacked-exterior-prestream-20260910. Validation is restricted
to Release Hijacked route/catalog suites and the Sunrise DLL. Added coverage
checks retirement before intro136, unchanged cave request timing, no reactivation
on return, temporary-world/read deferral, and late entity readiness after an
authenticated actor/parent origin. Fresh live acceptance must verify the original
exterior population disappears before streaming and remains absent on return.


## Crash correction and load-zone timing - 10 September 2026

LIVE READ: candidate hijacked-exterior-prestream-20260910 faulted in PID64952
when its first exterior deletion ran from camera update at 175641 ms. The real
StackWalk64 trace reached 56B550 -> 56C4F0 -> 56A8F0 -> component callbacks ->
98F66, then the native fatal-exception wait. The allocation service obtained
from TlsGetValue(index at20BBB30)+58 was null. A main-thread ID and readable
world tables did not establish the required native context. Replacing56B550
with56A8F0 in the same camera callback alone would reproduce the same failure.
Evidence: build/coo/hijacked-prestream-hang-64952/walk-stack.json and
build/coo/hijacked-retirement-research-20260910/prestream-crash-context-handoff.json.
The previous candidate is failed live acceptance, regardless of its offline pass.

USER REQUEST: exterior Vex remain fightable when entering and leaving the tunnel
before the next load zone. Lua retirement therefore waits for actual Mists
intro136 again. Objective and Sagira patrol dialogue still update at tunnel mouth.
The route fixture now enters the tunnel, turns back, accepts an authentic exterior
death, and checks that remaining enemies retire only at intro136. Backtracking
after that point cannot reactivate the mission's retired source requests.

IMPLEMENTATION: camera/constructor polls only retain exact identities. One scoped
native hook at4EC1A0 snapshots the exterior source's complete owned weak list
before native generation retirement detaches it. The actual argument is checked
against the current run, catalog definition and source self references, including
incoming generation but not the still-old sense generation. Only retired Hijacked
surface sources1..7 authorize removal. Exact actor/parent origins and entity
salt/bundle/world checks remain. A nonblocking guard prevents recursive dispatch;
no placement/cache mutex is held across native marks. All paths forward the
original routine, including unrelated sources and missing allocator context.

Before manual56A8F0 mark/remove, the current thread must have the verified native
TLS allocator service and executable vtable methods+08/+10/+20. The engine performs
subsequent full teardown; this adapter no longer invokes56B550. An unavailable
service defers manual work. Source-boundary logs record owned count, capture
count and allocator readiness; entity logs record mark requests and returns.
Hook installation/removal is atomic with the existing placement hook and shares
its CallGate. No authority bitmap or TLS service pointer is written.

Candidate: build/coo/hijacked-exterior-native-boundary-20260910. Validation scope
is Release hijacked_tests, hijacked_catalog_tests, and Sunrise DLL only. Offline
checks cannot prove native allocator availability at every source callback or
recover lineage for completely reconstructed ownerless actors. Fresh gameplay
must verify crossing intro136 without a crash and returning outside without
surviving exterior reinforcements. A missing allocator is logged and deferred,
not treated as successful removal. Consult the candidate acceptance receipt for
build/install status and any later live evidence.


## User-marked inner-tunnel cleanup point - 10 September 2026

LIVE READ: native-boundary candidate9d96ea04 loaded correctly in PID65288,
including both placement/source detours. The current log recorded zero retirement
requests, source-boundary callbacks or manual marks. The graph remained at
mists.intro_trigger_volume (153E22CD/60/136); the new native cleanup path was
therefore never exercised. Source instances and two Harpies were recreated on
backtracking while all exterior source requests were still active. Evidence:
build/coo/hijacked-loadzone-investigation-65288/findings.json and sunrise.log.

USER LOCATION + LIVE READ: the requested cleanup point is (111.1,232.5,-80.7),
scenario bubble37, slice296. Memory agreed at (111.1387,232.5255,-80.7116).
Native intro136 lies farther inside: min(83.4811,270.1000,-82.4000),
max(95.0665,288.1825,-76.4000). It was incorrect to equate that mission volume
with the earlier location the user wanted. The later screenshot resolves timing.

Lua now has a separate exterior.cleanup condition: the user-directed local
region or native intro136 as a fallback for a missed local sample. The authored
AABB is min(104,230,-86), max(118,240,-74); it contains the user point and begins
beyond the earlier tangle endpoint (maxY228.4), excluding the tunnel mouth.
route_geometry.h labels these coordinates as authored from the user location,
not recovered package geometry. The local observation is registered as a module
capability and latched per run. It cannot make any native memory mutation itself.
The earlier objective/VO remains at the tunnel-mouth cue. Cave entrance spawns
still require their original intro136 observation. No kill or backtrack action
is required; those are optional player choices, not mission objectives.

The existing guarded source-retirement hook is unchanged. New logging records
cleanup_point_entered once with run and actual position, followed by the existing
retirement request, source capture/allocator and native mark diagnostics.
Candidate: build/coo/hijacked-cleanup-user-point-20260910. Validation is restricted
to Release Hijacked route/catalog and Sunrise. Focused coverage checks the exact
user point, no early cleanup, zero-kill retirement before intro136, run reset,
and independent cave progression. Native disappearance still needs live evidence;
changing the travel cue alone cannot establish native mark success.

## Native unload selection correction - 10 September 2026

LIVE READ: candidate hijacked-cleanup-user-point-20260910 failed native cleanup
in PID46916. The local cue fired at130234 ms, position(111.842,230.001,-80.764),
and all seven source retirement requests followed at130297 ms. At130484 ms the
native region was already33. The first4EC1A0 source boundaries arrived only on
return at145844 ms, with new source handles and empty owned lists. All14 retained
original entity identities had been replaced; no entity mark was requested.
The returned14 actors were ownerless with no source definition, authored table,
record or GUID linking them to the original enemies. Evidence is retained in
build/coo/hijacked-cleanup-live-46916. Do not recover their ownership by proximity,
species, actor slot index or transferring an expired entity salt.

NATIVE CODE:424D10 owns area unload; EDX is the destination region (Mists264).
It calls A07BE0 before the general572B50 selection and572F70 teardown sweep.
A07BE0 calls557690 for bundle component80807E3E. At return addressA07CBE it
consumes only AL and, when true, calls56A8F0 itself. The earlier C79440 callback
removes authored table records through569D10; the inspected path is not an actor
serialization export. No claim is made about other persistence stages outside
this recovered chain. Full evidence and ABI notes are in
build/coo/hijacked-retirement-research-20260910/native-unload-discard-handoff.json.

IMPLEMENTATION: two additional interceptions (424D10 and557690) share the
existing placement CallGate and atomic detour lifecycle. A scoped thread-local
unload frame snapshots authenticated exterior sources1..7 identities before
forwarding native unload. It only permits selection toward region264. Nested
unloads mask the parent frame. The component lookup always forwards its original
call and preserves its output bytes. Only the verified boolean-only callsite may
return true for the same run, world, exact entity address/full salt and bundle,
with no conflicting actor owner and a valid native allocator TLS service.
The engine performs mark/remove and its normal teardown; this path does not call
native deletion manually or write a component, object-authority bit or TLS slot.
Installer guards verify both entry prefixes, the native call/branch bytes and
the component type. Quiescing forwards originals and suppresses added selection.

Area cleanup permits a managed source that is active or retired, so it remains
independent of kill counts, objective section, and Lua publication timing.
The earlier local cue still retires the exterior source requests, and the
tunnel-mouth objective/VO and native intro136 cave activation stay unchanged.
Enemies remain available for optional combat before the actual area transition.

Diagnostics record unload_begin/end with destination, retained count, predicate
count, selected count and rejected count. unload_selected records each exact
target; *_after records the native read state (0 unreadable,1 other world,
2 replaced,3 removed,4 still live). A changed world alone is not disappearance
proof. unload_rejected reasons are1 lease/source,2 nonlive read,3 identity/callsite,
4 conflicting actor owner,5 missing allocator,6 final lease/identity recheck.

Candidate: build/coo/hijacked-native-unload-cleanup-20260910. Validation is limited
to Release hijacked_tests, hijacked_catalog_tests, and Sunrise DLL. Added focused
checks reject wrong regions/callers/component types, stale runs, foreign sources,
wrong entity addresses/bundles, reused salts and changed worlds. Existing route
checks retain optional pre-transition combat and zero-kill cleanup. Offline
checks and installation do not establish disappearance on return; a fresh run
must verify native selection and absence of reconstructed exterior enemies.

## Temporary exterior Goblin suppression - 10 September 2026

USER REQUEST: abandon further cleanup investigation for now and disable only
the exterior Goblin reinforcements accompanying the Harpies. Keep the Harpies
and Hobgoblin. The user reports that the native-unload candidate also failed
to keep enemies absent on return; it is not accepted as a gameplay fix.

Lua publishes surface.mists_goblin01/02/03_squad (3E9B74F3 sources3/4/7) as
retired with zero counts from mission start and never requests them. This
prevents their initial spawn instead of depending on successful despawning.
The Harpy requests (sources1/2/6), Hobgoblin approach request (source5), cave
population, tunnel objective/dialogue and the rest of the route keep their
existing schedule. The unused lower-bowl spawn dependency is removed. No
native hooks or catalog counts are changed by this workaround.

Candidate: build/coo/hijacked-exterior-goblins-disabled-20260910. Run only
Release hijacked_tests, hijacked_catalog_tests and Sunrise. Existing opening
coverage now checks that all three Goblin sources stay retired without any
admissions through approach, tunnel entry and backtracking, while the Harpies
and Hobgoblin still spawn and cave progression needs no exterior kills.
