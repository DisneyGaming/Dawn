# Deep Storage implementation record

Based on [MISSION-IMPLEMENTATION-TEMPLATE.md](MISSION-IMPLEMENTATION-TEMPLATE.md), 9 September 2026. This record is frozen with the candidate source. Generated validation, package, installation and acceptance receipts supply final delivery status without changing that source manifest.

## 1. Mission brief

- **Mission / content:** Deep Storage, Curse of Osiris, installed 2017 PC content, normal story variant; solo reconstruction.
- **Identifier:** `deep_storage`; package `adventure_whisk`.
- **Scope:** Rupture landing, entrance plate and Ghost scan, Pyramidion descent, warp arena and corridors, Network Protection, final two-plate encounter, scan, coordinates exchange and success. No next destination is shown.
- **Reference:** [VV0h2nulU0k](https://www.youtube.com/watch?v=VV0h2nulU0k), full 00:00–11:15 span; mission approximately 00:07–11:10. The user supplied the dialogue transcript. Auto subtitles and sampled frames are retained locally.
- **Current state:** Complete scripted mission with two joint live fix passes. Both scans progressed automatically in the second run. The user confirmed closing dialogue, Mission Complete and retained hologram after an approved ownership acknowledgement repair. This candidate retains those fixes and adds all descent requests at door opening, independent final plate waves, central beam removal after box destruction, and permanent removal of the unused hallway wall. Early final-room presentation, trigger-volume traversal, Hydra-only portal gating, and progression with optional enemies alive are retained. Fresh native acceptance remains pending; offline replay is not live acceptance.
- **Preserve:** Existing Gateway, A Deadly Trial, Beyond Infinity, Omega, shared boundaries, user settings and unrelated working-tree documents.
- **Required mechanics:** All transcript exchanges, held Ghost interactions, native timer completion, combat readiness/deaths, staged platforms, separate barriers/lasers/covers, native portals, objectives, final restriction and completion.
- **Checkpoint / death / retry:** Final-room restriction is published. New activity selection or mission reset allocates a new lease and clears observations. Same-room wipe/checkpoint restoration is **UNRESOLVED**; no native checkpoint restoration is invented. Relaunch from the opening is the implemented full retry path and needs live acceptance.
- **Deferred / excluded:** No type43 Scene exists in recovered mission groups. Optional gate reminder row8 is bound but unscheduled because it is absent from the supplied run. Exact retail counts, combat music cue scheduling and same-room checkpoint restoration remain fidelity gaps. Native ambient audio is retained. No generated Forest route is present.
- **Live work:** The first run used two approved scan-observer repairs. The second run completed both scans normally; user-requested pit barrier and ending repairs established the remaining fixes. Each intervention and its acceptance limits are recorded in sections9/11.
- **Delivery:** Source, complete validation, matching package/symbols, installation with game closed, rollback evidence and this record.
- **Authorization:** “watch this, please follow the template to a tee. Implement the entire mission, then we will go through and fix shit. You can use one high agent.” One high-reasoning agent recovered, tested and reviewed bindings. The attached document is a reference; this current request determines authorization.

### Starting request

Implement the complete supplied mission using the current Lua executor and native services, reconstruct bindings from installed packages, review the complete reference span, preserve existing behavior and prepare for the joint live fix pass. Do not turn offline simulation into a live acceptance claim.

## 2. Baseline and evidence

- Workspace `C:/Destiny 2 Development`, branch `main`, starting commit `da5c45dbc87938d463fe568c1ce5142cc1a996fd`.
- Preserved pre-existing changes: modified `Sunrise/docs/NEW-MISSION-RECONSTRUCTION-GUIDE.md`; untracked `IMPLEMENTED-GAPS.md`, `Sunrise-Implemented-Gaps-MDs-2026-09-09-035039/` and the user-supplied template.
- Executable `C:/Destiny 2 Development/destiny2.exe`, SHA-256 `81964380664e7fcee3c620085a157fdeaf91fefacf7214907820f188bbeb4ced`.
- Prior installed DLL SHA-256 `b5ac159c2b742065ebe5bcea2c158777a6906a2555aeaf85cfe039d8ce778101`; PDB `0d19c651f6fc66dd5e2aba1cad957168b913403ce27f108eb009461834ff379d`.
- No prior Deep Storage script. Existing script/settings hashes: [baseline.json](../../build/coo/deep-storage-research/baseline.json). Prior receipt: `build/coo/beyond-infinity-combined-fix-20260909/installation.json`.
- Packages: `C:/Destiny 2 Development/packages`; current native cache version53. Extractor records raw tag hashes.
- Research: [research-summary.json](../../build/coo/deep-storage-research/research-summary.json), [native-bindings.json](../../build/coo/deep-storage-research/native-bindings.json), tactical joins, spawn candidates, transporters, device resources/GUIDs/mode ranges, 129 raw tags, `reference.en.vtt`, `reference.info.json` and `reference.mp4`.
- **VIDEO:** All seven contact sheets were inspected, covering the whole span at five-second intervals; closer samples cover interactions and ending. This was full-span sampled visual review plus subtitles and the submitted transcript, not continuous audio playback. No off-camera teammate was identified. Charge timing is approximate.
- **PACKAGE:** Recovered native identities/descriptors/modes. **OFFLINE TEST:** synthetic receipts, parser/encoder tests and builds. **ASSUMPTION:** reconstructed host policy. **LIVE READ / LIVE INTERVENTION / USER CONFIRMED:** both live runs and their bounded repairs are recorded in sections9/11. No fresh run of the final-room candidate has occurred.
- Initial candidate: `build/coo/deep-storage-implementation-20260909`; previous validated candidate: `build/coo/deep-storage-trigger-flow-20260909`; current candidate: `build/coo/deep-storage-door-waves-beam-20260909`. Its generated `results.json`, `failures.json`, `source-manifest.json`, `package.json`, `delivery.json`, `installation.json` and `acceptance.md` establish final status.

The reconstruction, Lua authoring and shared-services guides were read and compared with current source. Current runner: 20 suites in Debug/Release plus both DLL builds, 42 results and five scripts.

## 3. Architecture: reuse before extending

[deep_storage.lua](../scripts/deep_storage.lua) owns seven phases, prerequisites, waves, dialogue order, objectives, device changes and completion. [bindings.h](../src/state/activity/deep_storage/bindings.h) exposes 440 named native capabilities; it is not a compiled story contract. Catalogs retain package identities and geometry. The controller composes existing executor, lifecycle, object, population, dialogue, objective and native activity-clock services.

### Capability decision records

- **Launch/authority:** Reused forced/prelaunch profiles and strict roster admission with this mission's metadata. Descriptor class, scope, offset and authority schema are verified; capacities unchanged. Catalog/encoder tests and full DLL builds cover integration.
- **Objects:** Reused inactive preparation → fresh-generation creation → salted entity acknowledgement. Added observers at existing `9EFFC0/9F0750`. Type23 uses existing native authority. Source acknowledgement and visible device behavior remain separate.
- **Plates:** Extended existing `1006F20` timer boundary and checked clock/apply/device primitives with three bindings. Player occupancy uses recovered geometry. Living admitted Vex positions are sampled on the world thread with actor/source generation/entity checks. Shared read-only `PopulationService::living` reuses the death ledger. Actual progress and original completion latch are required; the host does not write completion.
- **Scans:** Extended existing `E4A590` after-call observation with exact source/link/controller identities and effective native-duration playback guards (entrance5 seconds, final9 seconds). Native user input and Ghost binding remain in charge. The observer does not summon Ghost or complete on proximity. It requires a participating Ghost at start and subsequent native completion.
- **Combat:** Reused `A0D510` admissions and `C72390` real-health deaths, plus health/AI/tactical readiness. Added Deep Storage routing. Empty or unready sources never clear by time.
- **Dialogue:** Reused shared bank queue and submission boundary. Queue acceptance, submission and acknowledged-duration completion remain distinct; no duplicate voice owner.
- **Restriction:** Reused mission-director/lifetime schema with this scenario's bubble ordinal19. Omega retains ordinal14. Shared encoder tests decode these fields.

**New intercepted engine addresses: zero.** Helpers share existing original calls, CallGate ownership and teardown. `E4A590` teardown protects the added observer. Plate setters execute on the verified native timer thread. New mission data and helpers stay grouped under Deep Storage.

## 4. Reference reconstruction and native map

### Activity and launch binding

**PACKAGE:** activity295; investment `550500EE`; root `E6E910D2`; activity tag `80B56019`; scenario `80B5606D`; launch `80F9F35E`; 22 bubbles. Rupture bubble4/slice32; Pyramidion bubble19/slice152. Spawn `3AE5AC33` has three rows near `(1134.5,554.8,12.4)` inside the opening VO volume. **ASSUMPTION:** this geometric join is the intended launch selection. The user reached and charged the entrance plate in the original candidate; exact launch placement remains a fidelity check.

### Beat records

The following records share these policies: crossings are retained after opening observation, plate occupancy is current, repeated crossings do not replay completed steps, and no return visit is authored. Activity reset clears observations and reserves a fresh lease. Final checkpoint restoration is unresolved. All gates are **OFFLINE TESTED**. Original-build live progress is recorded below; revised visual and scan behavior still requires a fresh run.

#### B01 — opening, The Rupture

- **VIDEO/objective:** 00:07–00:42, Enter the Pyramidion.
- **Action/preconditions:** Land; valid opening position starts retention.
- **Visual/dialogue:** Entrance plate and blocks; objective0 and row0, shared queue.
- **Native input/output:** Opening volume plus prepared/created type4 sources.
- **Gate:** Real opening position; plate bindings must exist before arming. Early movement is retained; reset retires the attempt.

#### B02 — entrance plate

- **VIDEO/objective:** 00:42–00:58, Enter the Pyramidion.
- **Action/preconditions:** Stand on the prepared, bound, uncontested plate.
- **Visual/dialogue:** Native charge, conflux reveal, objective1; row1 “trick with that conflux.”
- **Native input/output:** Entrance plate source/volume/timer; conflux source and channel.
- **Gate:** Native progress then completion latch; occupancy alone cannot finish. Interrupted charge resets; completed charge survives departure.

#### B03 — scan, door and tunnel

- **VIDEO/objective:** 00:58–01:57, Enter the Pyramidion.
- **Action/preconditions:** Hold Ghost interaction after conflux reveal, then enter.
- **Visual/dialogue:** Scan and door opening; rows2,3,4 in one queue.
- **Native input/output:** Entrance type65 sensor and Ghost device; tunnel VO volume.
- **Gate:** Real scan start then finish; door request follows completion and immediately requests all17 descent sources. Admission may occur later as native streaming loads the region; it does not block the opening phase. Tunnel crossing is retained while earlier speech queues.

#### B04 — descent platforms

- **VIDEO/objective:** 02:06–03:57, Descend into the Pyramidion.
- **Action/preconditions:** Traverse six groups from recovered approach volumes.
- **Visual/dialogue:** Staged blocks4–6; all17 population sources requested when the entrance door opens, before platform approaches. Row5 plays at the exit hallway. The unused descent wall is removed from opening setup onward.
- **Native input/output:** Descent source/volume set; separate exit-wall channel.
- **Gate:** Platform and exit-hallway volume crossings; descent enemies may remain alive. Fast approach reveals platforms without waiting for dialogue.

#### B05 — warp arena

- **VIDEO/objective:** 04:18–06:36, Traverse the warp gate network.
- **Action/preconditions:** Cross the entry, middle and north arena triggers, defeat the portal Hydra, then touch the portal.
- **Visual/dialogue:** Frame candidate .1; contact barrier released; rows6/7.
- **Native input/output:** Warp sources, frame23:38, barrier23:39, first receiving volume.
- **Gate:** Only the portal Hydra death, then current receiving occupancy. Standing beside the source gate is insufficient.

#### B06 — warp corridors

- **VIDEO/objective:** 06:36–07:38, Traverse the warp gate network.
- **Action/preconditions:** Follow native portals and cross the laser corridor.
- **Visual/dialogue:** Four laser trap sources, four defender sources, row9 reputation exchange.
- **Native input/output:** Native map transporters and second endpoint volume.
- **Gate:** Endpoint observation, independent of corridor deaths; no host teleport. Native portal direction joins remain incomplete.

#### B07 — Network Protection

- **VIDEO/objective:** 07:38–09:14, Overcome the Vex.
- **Action/preconditions:** Enter Cyclops approach, defeat boss/defenders, drop into pit.
- **Visual/dialogue:** Pit barrier closed at native mode0, Cyclops and three guard groups; mode1 removes the barrier after boss and all three groups die. Final plate/lens/beam/catch/cover sources also load at the approach. Row10 plays at final descent.
- **Native input/output:** 19 sources, pit barrier, final-route/floor volumes.
- **Gate:** Cyclops plus defenders dead, then pit/floor arrival. Final restriction enables here.

#### B08 — two final plates

- **VIDEO/objective:** 09:28–10:27, Activate the conflux.
- **Action/preconditions:** Final-room objects already loaded at Cyclops approach; charge both plates while Vex are present.
- **Visual/dialogue:** Final plates begin red with rods and side beams visible; each completed plate removes its own rod/beam/catch. Each plate starts its own five-source wave on occupation, then five reinforcement sources after those first-wave deaths; row12 and center simmer run once on first occupation.
- **Native input/output:** Two timers, 21 source groups, beam/catch/cover controls, and the source-owned `80F48026 / 80804B8A / +B08` lens health component.
- **Gate:** Both plate latches expose the box immediately, independently of remaining combat. Actual destruction of the bound box permits reveal without requiring final-room enemy deaths. Contest interrupts incomplete charge; solved plate stays solved.

#### B09 — final conflux

- **VIDEO/objective:** 10:27–10:55, Activate the conflux.
- **Action/preconditions:** Both plates charged and box destroyed through native damage; climb and hold scan. Remaining enemies do not block access.
- **Visual/dialogue:** Lens/cover changes and conflux reveal; row11.
- **Native input/output:** Final sources/channels and type65 link `EA42F517/0`.
- **Gate:** Native scan start then finish. The final scan explicitly ends the encounter and retires unfinished wave waits; it cannot stall on optional enemies. Final stairs and covers need independent visual acceptance.

#### B10 — coordinates and ending

- **VIDEO/objective:** 10:55–11:15, coordinates found / mission end.
- **Action/preconditions:** Complete final scan and remain through closing exchange.
- **Visual/dialogue:** Probability dome, rows13/14 and mission success.
- **Native input/output:** Bank rows13/14, hologram source/channel, lifecycle completion.
- **Gate:** Full acknowledged row14 duration before success. No next destination is authored; restriction and marker clear at completion.


### Native binding records

Complete records in `catalog.h` and `native_catalog.h` preserve registry, definition, type/slot, offset, runtime class and sense/auth schemas. Raw evidence and hashes are in `native-bindings.json`. Principal bindings:

- **Entrance plate:** `A13D8A45 / 80B560D5 / 4:4 / +4C8`; volume `A13D8A45 / 80B5604C / 60:12`. Entity `80C6BE61`; generic `80C7063B / 80803910 / +A78`; timer `815B8B3B / 80804FCB / +248`. Source runtime is `80809928`, package component class `80809927`. Source and committed generations must match before salted entity/generic/timer handles bind.
- **Final plates:** `59700FA7 / 80B56868 / 4:72` and `59700FA7 / 80B56875 / 4:74`; volumes `59700FA7 / 80B56378 / 60:273` and `60:280`. Same native resources independently recovered from each source. Completion is per plate.
- **Entrance scan:** Source `A13D8A45 / 80B5609E / 4:2 / +4C8`; entity `80F4B127`; link `4324A238 / 80B56174 / 65:0 / +258`; controller `8156EFA4 / 80804D3A / +358`; GUID `CDFC784B0C4A9CF5`.
- **Final scan:** Source `59700FA7 / 80B5645C / 4:95 / +4C8`; entity `80F4B586`; link `EA42F517 / 80B5654E / 65:0 / +258`; controller `8157E6B1 / 80804D3A / +358`; GUID `844920F35A95B55B`. Catalog runtime class `80804D3B` differs from the actual live definition reference `80804D3A`; the observer checks the latter as the existing native reader does.
- **Ghost scope:** Package link `+288` is registry/type65/slot0; `+290` bubble4/19; `+298` selector `811C9DC5`; GUID at `+2B0`, not `+2B8`. Both controller resources have a default duration of3 seconds at package `+6D4`. Source override class `80804D39` at `+614`, record `+618`, supplies effective duration at `+638`: entrance5 seconds, final9 seconds. Native controller `+290` contains the effective duration after this override. Device mode2/active/elapsed are read after `E4A590`; a participating Ghost component `80803F45` must first be seen running.
- **Warp:** `59700FA7 / 80B566AA / 23:38`, static entity `80C3EFD8`, graph `80F27377`; barrier `59700FA7 / 80B566B0 / 23:39`. Definition class `80804F45`, auth `80804F48`, generic live definition `80803910`. Publication requests a position; it does not prove rendering.
- **Network Protection barrier:** Source `59700FA7 / 80B566FA / 4:49 / +4C8` creates entity `80C347E1`; channel `59700FA7 / 80B566FD / 23:50 / +278` controls generic device `80C22346 / 80803910 / +A78`. Closed is native position0 and removed is1. Exact-asset mapping applies before the generic inactive return, preserving logical active/desired state. Real Cyclops and three-wave deaths release the channel; the barrier is not a destructible.
- **Dialogue/objective:** Root `E6E910D2`, dialogue `80B565E2 / 53:2 / +1408`, bank `80F1EEB6`, auth `80804F77`; directive `80B565DF / 68:0`, auth `80804F67`. Native objective events/text are retained. Active markers bind entrance/final scan sources rather than fabricated coordinates.
- **Final geometry:** Lens23:90, conflux23:91, hologram23:92, covers23:83–86, lasers23:87–89 and catches23:93–94 in `59700FA7`. Source objects and channels are separate. Exact tags/graphs are in `mechanism_catalog.h` and sidecars.
- **Population:** 72 type1 sources in `59700FA7`; runtime `8080948F`. Definitions/offsets, category selections, placements and tactical joins are in `ai_bindings.h`. Actor handle, source handle/offset, scope, generation/committed generation and real health death authenticate receipts.

Common-looking resources were verified through each mission source's own references. No numeric source was copied solely because another mission looks similar.

## 5. Triggers, native scenes, and dialogue

### Trigger record

57 recovered polygons and vertical ranges are sampled against authenticated local-player position. Containment handles concavity/inclusive edges and rejects nonfinite positions. This is sampled geometry, not an engine trigger callback. Coordinates remain in package world space. Retention begins at the Rupture opening volume; early crossings survive queue/phase prerequisites. First portal receiving occupancy and all plate occupancy are current. Reset clears current and retained observations. No second visit is synthesized.

### Scene record

**PACKAGE:** No type43 Scene descriptor exists in the mission-owned roster. Type65 Ghost interactions and native objects supply performances. Existing Scene infrastructure was inspected and retained; no Scene speech is duplicated in the queue.

### Dialogue records

Every row below belongs to bank `80F1EEB6`, shared queue, with native selector/variant data in the catalog. Deduplication uses run and bank generation. Completion starts at actual native submission and uses recovered bank duration plus shared spacing. Request time alone cannot finish playback. This proves scheduling in tests; audible playback remains a live check.

- **0 / 9277ms:** Opening map question and Ikora answer; valid landing observation.
- **1 / 2374ms:** “I know a trick…”; charged plate and created conflux, after0.
- **2 / 2667ms:** “Brace yourself…”; real entrance scan start, after1.
- **3 / 4190ms:** “Or not” / “With me…”; completed scan and door request, after2.
- **4 / 38582ms:** Exile/change exchange; interior VO volume, after3.
- **5 / 12272ms:** “My scans show…”; descent exit hallway crossing.
- **6 / 4538ms:** “I can reprogram…”; warp arena approach.
- **7 / 4715ms:** “Calculations are ready…”; portal Hydra dead and warp released.
- **8 / 4329ms:** Optional gate reminder; bound, absent from reference schedule.
- **9 / 17308ms:** “That didn't work…” and reputation exchange; first receiving corridor.
- **10 / 6008ms:** Cloaked conflux update; final pit arrival.
- **12 / 2281ms:** “Look out…”; first final plate occupied.
- **11 / 1110ms:** “There we go”; both plates, native box destruction, and combat complete; conflux revealed.
- **13 / 4230ms:** “The map's not here” / “Keep looking”; final scan complete.
- **14 / 7754ms:** Coordinates exchange; row13 finished, own finish gates success.

Completed one-time graph steps cannot request a row again. Scripts reload only in a new process. Fast-traversal replay verifies `0,1,2,3,4,5,6,7,9,10,12,11,13,14` in order while movement triggers arrive early.

## 6. Devices, plates, destructibles, and clocks

### Mechanism records

- **Preparation/creation:** Managed type4 starts inactive at its lease. Real inactive callback permits creation at lease+1. Active/committed generation, salted entity and component owner must match. Source acknowledgements do not establish visual acceptance.
- **Timer/clock:** Entrance charge5 seconds and final charges10 seconds are **VIDEO estimates**. Native normalization end is0. Existing checked clock encoding uses673200 ticks/sec; original timer yields progress/completion. Host elapsed time is not completion.
- **Interruption/contest:** Departure or a living admitted Vex in the volume revises and stops/restarts incomplete charging. Completed plates stay latched. Position reads revalidate actor/source/entity; incomplete enemy samples preserve previous contest rather than clearing by absence.
- **Plate presentation:** Entrance plate0 preserves idle0, occupied.1, contested.2, completed.1. Final plates1/2 use preloaded/unarmed/waiting/contested.2, charging.1, completed0. Sources and presentation activate during the preceding Cyclops encounter; timers remain stopped until final-room arming. Arming is part of the native timer command identity, so unchanged occupancy cannot suppress the start. Each completed final rod stays removed after departure/reentry and success. Current/target reconciliation uses the native setter and retains the completed timer. The corrected final presentation requires fresh visual acceptance.
- **Warp frame:** Graph supports near0, .0999–.1999 and .1999–.2999. Position1 matches no branch. Active candidate is.1; visual direction unverified. The exact descent wall E6402111/80B56A48/23:0 always uses removal.2, regardless of the logical active input. Native0 raises it; this wall is never used by Deep Storage.
- **Barriers:** Warp direction remains an assumption. The pit barrier direction is recovered: graph `80F567A6` mode0 writes scalar1 and raises the second visual output, while mode1 uses `(0,0,-1,1)` cubic coefficients to fade both outputs to0. Mode0 selects event `CE330D3E:0F0B176F`, subscribed by rendered model `80F4BF9A` and physics resource `80F4BF9B`; mode1 selects removal state `1575D743`. Secondary graph `80F4BF9C` creates effect `80C2A1CB` only in the closed0 range. Mode.5 has no actions. Evidence: `build/coo/deep-storage-pit-barrier-live-20260909/pit-barrier-mode-proof.json`. Entrance channel is recovered; static entity join unresolved.
- **Final puzzle:** Plate/lens/block/laser sources exist initially; lens shielded1, side beams/catches on, center beam off, covers on. Each solved plate removes its rod and side beam/catch. Both plates publish a fresh lens-device revision at exposed.75 while preserving its source. Native owned box destruction removes the central beam, lowers covers and reveals the conflux independently of combat. Dome follows scan. Stair/cover geometry still requires acceptance.
- **Destructibles:** The user corrected the final puzzle: the box becomes breakable after both plates. Source `59700FA7 / 80B568A6 / 4:79 / +4C8` creates entity `80F4803C`, containing shared box health `80F48026 / 80804B8A / +B08`. Health operator `80B56920 / 26:96 / +AC8` and object collection `80B56205 / 34:120 / +388` identify the lens. Existing native damage hooks authenticate this exact source, lifecycle, committed generation, entity serial, and health handle. Only the native death bit `+338 & 1` on the bound exposed box releases the destruction gate. No health value, destruction bit or completion latch is written.

Mode ranges were recovered before choosing values. A branch proves a native mode exists, not its appearance. Check initial state, beams, plate border, covers, stairs and final dome independently. The pit correction additionally follows decoded animation actions and shared model/physics event states. Regression tests inspect the native position word/revision and replay both Cyclops-first and Cyclops-last deaths: boss-only or waves-only clearance keeps the pit closed. Targeted Debug/Release results are in `build/coo/deep-storage-pit-barrier-tests-20260909/`; automatic live removal is not yet accepted.

## 7. Portals, receiving areas, and generated routes

### Portal records

- **Warp arena → first hall:** VIDEO 06:26 reveal / 06:36 travel. Frame23:38/barrier23:39 release after the portal Hydra dies; remaining arena adds do not block it. Receiving `E86A5BFD / 80B5655C / 60:2`, near `(783–800,552–573,-618–-598)`, requires current occupancy. No host teleport. Actual transporter linkage and frame appearance need live confirmation.
- **First hall → laser corridor:** VIDEO 07:04. Fixed Pyramidion portals/map geometry own transport; no fabricated teleporter source. Row9 may finish while moving. Four laser traps and four defender sources populate the corridor. `native-transporters.json` records partial joins and unresolved directions.
- **Laser corridor → Cyclops slope:** VIDEO 07:28. Native transport owns movement. Second endpoint `54DA5E1B / 80B565A5 / 60:1`, near `(349–367,528–553,-716–-696)`, plus Cyclops approach volumes gate progress. Load boundaries are not treated as contact. No return trip is authored.

Complete transporter chain has not been proven live or fully joined statically. Existing transporters run and the mission observes destinations. Test waiting beside the source barrier, then contact; validate frame/core, placement/orientation, destination and arrival independently. No reflection Scene or generated route is present, so the procedural Forest subsection does not apply.

## 8. Encounters, environmental performances, and ending

### Encounter records

- **Descent:** All17 sources across six platforms are requested by the door-opening step. The exact descent-only `.request` capabilities use requested completion, retaining the source until streaming admits native actors; `.spawn` elsewhere still waits for health/AI/tactical readiness. Ten empty direct rule selections use recovered matching named spawn-rule definitions, explicitly a reconstructed join. Blocks4–6/covers appear on preceding approach volumes. The unused descent doorway wall is forced to native removal mode.2 from opening setup onward; surviving platform enemies do not prevent advancement.
- **Warp:** 11 sources: approach Harpy; Goblin/Hobgoblin; six Minotaur/Harpy sources; Hydra/Goblin. Entry/middle/north native-volume conditions request each stage independently of deaths. The Hydra alone gates the portal.
- **Corridor:** Four Hobgoblin/Harpy/Goblin sources and four laser traps; route continues on the receiving-volume observation with survivors allowed.
- **Network Protection:** Cyclops plus18 defender sources in three groups. Approach/middle/front volume conditions request the groups independently. Boss and defender clearance are independent and both required for pit release.
- **Final:** 21 sources: three simmer, four starter fanatics, eight later fanatics and paired Hydra/Harpy/Minotaur groups. Each plate occupation requests its own first wave; only that wave's authenticated source deaths release its second wave, independently of plate charge or the other side. Final scan completion cancels unfinished wave branches, so enemies do not gate the ending. Reinforcements are finite; an endless retail host respawn loop was not recovered.
- **Counts/behavior:** **ASSUMPTION:** one actor per native category:68 sources with one category, four with two, 76 planned actors. Exact retail counts were not recovered. Native category/rule/placement retained. Tactical selection is reconstructed from provider/placement joins. Exact retail difficulty/scheduling is not claimed. Readiness requires native health, AI and applied tactical assignment; actual combat and named boss appearance need live checks.
- **Shield policy:** No extra immunity collection inferred. Native archetypes retain their behavior; environmental barriers and enemy health are separate.
- **Environmental performance:** Shortcut, platforms, warp reveal, pit release, plate beams, covers, conflux and dome are separate native controls. A source acknowledgement or dialogue line does not prove visible performance.

### Ending contract

Approach, reveal, scan start and scan finish are separate. Row13 must submit and finish; the hologram source must then bind while its display is off before row14 submits. Display mode.5 rises to1 and retains the hologram; the previous mode1 fades to0. Dome appears during coordinates; success waits for full acknowledged row14 duration. Then restriction releases, marker clears and owned success publishes (phase6/success1). No next destination is shown. Sources retain final authored states until activity unload. Fresh ending: **NOT YET TESTED**.

## 9. Live experiment record and persistence

**Original-build live session:** Destiny PID44068, creation FILETIME134334411283635449, initial DLL SHA-256 `ca51e56a09def47f0e1a26eaa9a4b84e7862a848ac16a91a223f5c4da8df4f0d`. The user completed the entrance and final scans. Their native controllers reported effective5/9-second durations, but the original observer required3. The user explicitly approved each four-byte host-observer reconciliation separately. `approved-entrance-repair.json` records `00000000 -> 01000100`; `approved-final-repair.json` records `01000100 -> 01010101`. Owner, generation and completed native scan were checked before each repair. No enemy death, population, plate completion or mission-success state was forced. This run is intervention-assisted, not clean acceptance. Temporary addresses are evidence only and must not be reused.

The original process has exited. Its log and read-only captures are preserved under `build/coo/deep-storage-scan-live-20260909/`. Both plates and all final enemies completed; the final repair allowed closing dialogue13/14 to submit. Ending then stalled on hologram source4:82 `missing=object_creation`; mission success was not observed. The user reported the dome fading during dialogue and completed plate receivers disappearing on departure. Installer refuses replacement if Destiny starts; DLL and Lua changes apply in the next process. A source acknowledgement or dialogue submission is not visible/audible acceptance.

## 10. Tests, package, install, and acceptance

### Candidate checklist at source freeze

- [x] Mission selection, routing, project files, Lua and override button registered.
- [x] Focused catalog/full-route tests pass, including fast traversal, stale owners, plate interruption and reset.
- [x] Two new suites registered in full runner.
- [x] Fifth script included in package and installer/rollback lists.
- [x] Installer expects all 42 results; no bypass.
- [x] Position-routing and shared restriction-encoder regression checks pass.
- [ ] Full fresh Debug/Release run, package and installation: generated receipts establish the result after source freeze.
- [ ] Fresh-process log/fingerprint and live acceptance: NOT YET TESTED.

Focused records: `deep-storage-catalog-dev`, `deep-storage-tests-dev2`, `deep-storage-shared-dev`, `deep-storage-dll-dev` under `build/coo/`. Test harness compiler/readiness failures were fixed; development logs retained. No warning/failure is ignored.

Final validation runs 20 suites in both configurations plus two DLL builds. Packaging checks unchanged sources/binaries/symbols and includes source archive. Installer checks payload/baseline, backs up prior files, requires game closed and verifies installed hashes. Failure rollback restores backups and removes newly introduced files only under verified workspace paths.

```powershell
C:/Python313/python.exe tools/coo/extract_deep_storage_bindings.py --check
C:/Python313/python.exe tools/coo/generate_deep_storage_capabilities.py --check
C:/Python313/python.exe tools/coo/verify_lua.py --out build/coo/deep-storage-implementation-20260909
C:/Python313/python.exe tools/coo/package_lua.py --validation build/coo/deep-storage-implementation-20260909
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/deep-storage-implementation-20260909 -ValidateOnly
& ./tools/coo/install_candidate.ps1 -ValidationDirectory build/coo/deep-storage-implementation-20260909
```

[Final acceptance record](../../build/coo/deep-storage-implementation-20260909/acceptance.md) reports actual results, installed hashes, backup and evidence archive. A missing receipt means that step has not been established. This frozen document does not predeclare successful validation/install.

### Fresh-process playthrough checklist

For the revised installed candidate, these fresh-process checks remain **NOT YET TESTED**. Section11 records the narrower original-build observations:

- [ ] Rupture landing/control and initial plate/conflux states.
- [ ] Held entrance interaction binds Ghost and visibly opens door.
- [ ] All exchanges audible once/in order, including rapid movement.
- [ ] Platforms appear before needed; encounters fight and clear correctly.
- [ ] Each portal frame/core, contact behavior and destination.
- [ ] Corridor lasers, Cyclops combat/presentation, pit release/drop.
- [ ] Both final plates: clock, border, contest, interruption, retained completion.
- [ ] Each beam/catch, covers, lens, stairs, conflux reveal.
- [ ] Final scan, full coordinates exchange, dome, success.
- [ ] Darkness/death behavior; investigate same-room checkpoint recovery.
- [ ] Full run without forcing, memory holds or fabricated receipts.

## 11. Delivery record

- **Source:** [Lua](../scripts/deep_storage.lua), [controller](../src/state/activity/deep_storage/controller.cpp), [catalog](../src/state/activity/deep_storage/catalog.h), evidence and shared integrations above.
- **Launch:** Restart Destiny normally with installed Sunrise. Choose **Deep Storage opening** in Activity Override, then use the existing launch flow. Profile sets `adventure_whisk`, bubble4, slice32, spawn `3AE5AC33`. Installation does not change saved selection/settings.
- **Candidate/package/source:** Revised `build/coo/deep-storage-persistence-final-20260909/`; initial `build/coo/deep-storage-implementation-20260909/`; `lua-missions.zip`, `source.zip`; matching PDB and five scripts in `payload/`.
- **Actual validation/install/hashes/backup:** Candidate `acceptance.md`, `results.json`, `failures.json`, `package.json`, `installation.json`.
- **Research:** Candidate `deep-storage-evidence.zip` plus hash receipt preserves sidecars, raw tags, reference metadata and review sheets; local reference video hashed separately.
- **Live observations (run1, original installed candidate):** Entrance plate charged; both completed native scans captured; descent17 enemy deaths; warp-gate waves including Hydra cleared; both corridor transitions and its4 enemy deaths; Network Protection and defenders cleared; both final plates and all final enemies completed; closing dialogue13/14 submitted, then hologram creation stalled. Mission success was not observed. Log evidence is in `build/coo/deep-storage-scan-live-20260909/live-progression.log`. **Live interventions:** Separately user-approved entrance and final scan receipt reconciliations; see section9 and correction below. **Fresh unmodified full run:** NOT YET TESTED.
- **Assumptions/gaps:** Launch spawn, retail counts/tactics, device mode direction, complete portal joins, entrance static door entity, final stair/cover geometry, music cue schedule, same-room checkpoint recovery. First live check is landing → plate → held scan → open door; then proceed through the entire route and fix observed faults in source.


### Scan duration correction — 2026-09-09

The entrance playthrough exposed a permanent observer bug: `ScanPlayback::valid` required duration3, while the authenticated native entrance controller reported duration5, revision2, mode2, inactive, elapsed5.0031075. The player had completed the scan, but the rejected start sample prevented the host from accepting completion. This supersedes the earlier default-duration interpretation; it does not change the authored scan.

`E4BBB0` loads the base `80804D5A` record referenced by live controller `+160`; `E4BC42/E4BC45` copies record `+24` into effective controller duration `+290`. `E4BCB8–E4BCC7` then applies nonnegative override `80804D39 +20`. Entrance source `80B5609E` and final source `80B5645C` contain that override class at package `+614`, data at `+618`, and duration at `+638`: **5 and9 seconds**, respectively. Both base resources retain3 seconds at `+6D4`. `E4A590` observes elapsed `+29C` divided by effective duration `+290`, active `+299`, and revision `+294`.

The permanent fix accepts finite positive native duration and finite nonnegative elapsed. It retains matching generation, mode2, valid active state, a participating Ghost with positive running progress before recording start, and retained start before inactive playback at or beyond its effective duration can finish. Catalog metadata distinguishes default duration from source override; both generators verify the source override class and offsets. Bound scans waiting for start or finish now report an observation stall, while an absent native controller binding reports `controller_binding`.

Evidence: `build/coo/deep-storage-scan-live-20260909/scan-state.json`, `duration-findings.md`, `device-duration-owner.txt`, `duration-override-class.json`, and `package-duration-overrides.json`. The user explicitly approved an entrance-only reconciliation, recorded in `approved-entrance-repair.json`: four observer bytes changed from `00000000` to `01000100`, with the completed native owner/state revalidated. Dialogue2/3 and the door sequence subsequently progressed, and crossing advanced to section1. **That run is intervention-assisted and is not clean acceptance of the permanent fix.** The final scan later exhibited the same duration mismatch at9.00251865 seconds and received its separately approved reconciliation. The original process has exited. Both permanent scan corrections are included in the revised candidate; generated installation receipts establish delivery.

Regression coverage exercises effective durations5/9, exact completion boundaries and native overshoot, early completion, absent participant/start, stale generation, invalid active/mode, nonfinite/nonpositive duration, and nonfinite/negative elapsed. The full offline Lua route now passes its entrance/final scan receipts through `ScanPlayback` with5/9 seconds, rather than directly accepting fabricated completion flags. Targeted Debug/Release validation is recorded in `build/coo/deep-storage-scan-duration-fix-20260909/`; fresh-process native acceptance remains separate.


### Native enemy health reader correction — 2026-09-09

**LIVE READ:** The original observer rejected every examined descent Vex health binding because `coo_native::component` defaulted to256 reflected rows. Goblin/Hobgoblin resources contain432 and Fanatic resources453. Extending the read-only capture to1024 resolved a character `80806832` owned by the admitted actor, and its `80804BEE` health interface resolved to `80804B8A` with matching self/entity. Evidence: `build/coo/deep-storage-scan-live-20260909/population-1788968771.json` (rejection), `population-1788968797.json` (complete character/health identities), and `population-types.json` (installed PDB ledger layout).

The shared enemy readiness call now uses the iterator's existing1024-row ceiling. Generation, source, actor, character, health, alias ambiguity, bounded reads, and retirement checks remain. **OFFLINE TEST:** `coo_universal_services_tests` passes Debug/Release,692 checks each, including432/453/1024-row tables with the character beyond row256, foreign health entity, bad character self, and oversized1025-row rejection. Results are in `build/coo/deep-storage-health-reader-fix-20260909/`. The fixed reader has not yet run in a fresh game process.

The live clearance ledger continued accepting authentic deaths: all17 descent actors died, then the mission entered its warp-gate phase and spawned the subsequent waves. No encounter kill or population receipt was forced. Tactical-assignment verification was corrected separately as described below.


### Applied tactical assignment correction — 2026-09-09

**LIVE READ / NATIVE CODE:** All17 captured descent sources held their authored tactical row at requested `+234` and applied `+5FC`, while selected index `+600` was0. The old observer compared `+600` to the authored row, so only authored row0 passed. Native `4E2A90` copies requested scope to `+5F0/+5F4/+5F6` and authored row to `+5FC`, then resets selected index `+600`; `4E2C40` selects the latter independently. Native `4E8727/4E86EA` passes both indices separately to `AB4A60`. Evidence: `build/coo/deep-storage-scan-live-20260909/tactical-readiness-1788968918.json`, `tactical-authored-row-path.txt`, and `tactical-assignment-writer.txt`.

The shared observer now requires matching requested/applied registry, type, slot and authored row; it separately requires a nonnegative selected index and non-absent tactical group. It revalidates the requested/applied scope, both rows, selected index and group after reading health/AI state. The mission's authored tactical policy is unchanged. **OFFLINE TEST:** Universal services pass Debug/Release,721 checks each, including authored2/selected0, mismatched applied fields, unresolved selection/group, stale generation, and mutations between initial capture and readback. Results: `build/coo/deep-storage-tactical-readiness-fix-20260909/`.

**Intermediate validation:** `build/coo/deep-storage-live-fixes-20260909/` passed42 checks before the visual persistence edits. It is not the final delivery. The final stable-source validation/package/install destination is `build/coo/deep-storage-persistence-final-20260909/`; this source record is frozen during that validation.


### Completed plate presentation retention - 2026-09-09 (superseded for final plates)

**USER REPORT / PACKAGE / NATIVE CODE:** Completed plate beam receivers disappeared after departure. Graphs `80F569A4/80F56977` for plate entity `80C6BE61` distinguish `device_position` input0 from `timer_value` input1. The completed branch at `80F56977 +1618 -> +16A8` requires device position.1 and timer>=.9999. The old hook returned immediately once charged and did not reconcile later visual drift. The exact write causing that drift was not captured before the game exited.

The prior `plate_presentation.h` implementation retained position.1 for all charged plates. The subsequent user correction below supersedes that policy for final plates1/2; entrance plate0 still uses it. It checks both native current `+370` and target `+37C` so a pending return to idle is corrected through the existing `DF6BD0` setter, with revision `+960` readback and owner/generation checks. The timer is never reset after completion. Reconciliation runs before/after the existing native timer tick and immediately when native completion is observed. Charged presentation requests survive mission success and retire on reset/unload; uncharged occupancy/contest behavior is retained.

**OFFLINE TEST:** Current/target drift, stale owner, setter failure, revisions, completed/departed plate, mission-success retention and reset are covered. Native proof: `build/coo/deep-storage-plate-retention-20260909/plate-presentation-evidence.json` and `presentation-native-disassembly.txt`. Focused Debug/Release replay/catalog checks pass in `build/coo/deep-storage-persistence-tests-20260909/`. Fresh visual acceptance remains pending.

### Hologram persistence and ending ordering - 2026-09-09

**USER REPORT / LOG:** The probability dome briefly appeared then disappeared during the closing line. The original ending log repeatedly waited for source4:82 `object_creation`; no success receipt followed. Source is `59700FA7 / 80B568AF / 4:82 / +4C8`; channel is `59700FA7 / 80B5690B / 23:92 / +278`; entity/graph are `80F2625D/80F2625B`.

**PACKAGE / NATIVE CODE:** Display branch.5 (`.4499.. .5501`) action6 at `+1F98` uses bytecode `3c00220023220034000f2322003e00`, cubic constants `(0,0,1,0)`, ending at scalar1. Previous mode1 (>=.9499) action9 at `+2558` adds opcode `3501`; constants1/2 are1/0. Interpreter `3B99F5`, implementation `3B9A00..3B9A17`, performs interpolation, producing1-progress and ending at0. Thus the previous active setting explicitly selected the fade-out animation. The permanent active value is.5. Exact data/disassembly: `build/coo/deep-storage-plate-retention-20260909/hologram-animation-proof.json` and `presentation-native-disassembly.txt`.

Lua now finishes row13, creates/acknowledges the dome source while display is off, submits coordinates row14, enables the persistent display, waits for the full acknowledged row14 duration, then publishes success/releases restriction. Source and display stay active through completion until reset/unload. This removes enabling the effect before object creation is acknowledged. It does not force a source receipt or bypass ending dialogue.

**OFFLINE TEST:** Replay delays hologram creation, verifies coordinates/display/success cannot run before acknowledgement, exercises full dialogue completion, and confirms the same active source/device generations remain60 seconds after success. Catalog tests verify.5 belongs to the display range and excludes the fade-out branch. Fresh live dome retention and mission-success acceptance remain pending.

### Final plate and breakable-box correction - 2026-09-09

**USER CORRECTION:** Initial final plates must be red, with rods and beams visible. Each completed plate removes its rod and beam. Both plates make the central box breakable. This supersedes the earlier interpretation that final receivers should remain visible after completion.

**PACKAGE / NATIVE CODE:** Final plate graph `80F569A4` uses distinct .1/.2 branches: actions at `+2630/+2978` emit `EDDEE0F3` with constants0/1, respectively. Source `80B568A6 +580` selects lens entity `80F4803C`; its resource list joins shared health `80F48026`, generic `80C7063B`, and graph `80F48031`. The same graph and health ABI are already used by the Gateway/Beyond Infinity damage path: position1 is shielded, .75 exposed,0 removed. Existing callbacks `B804E0`, `CDCB60`, and `B7E3C0` now additionally route only the authenticated Deep Storage lens. A source-authenticated candidate protects the interval before the live health receipt binds; an already-dead candidate cannot manufacture a valid live receipt.

**SOURCE:** Both charge latches advance the lens-device generation and select .75 immediately; combat clearance is not a prerequisite to vulnerability. The original map graph separately waited for actual bound health destruction and required enemy deaths; the later trigger-flow correction removed the combat prerequisite to lens retirement, covers, and the final conflux. Final plate0/.1/.2 rendering is scoped by plate index; entrance behavior is unchanged. Final rods remain removed through ending and release ownership at reset/unload.

**VALIDATION STATUS:** Added source-identity/damage-gate fixtures, wrong generation/serial/health and dead-before-bind checks, full-route box gating and plate interruption checks, and independent native wire words for1/.75/0. Parent will record stable-source Debug/Release validation. The corrected final puzzle has not yet been played from its initial state. Evidence: `build/coo/deep-storage-final-puzzle-20260909/`.

**BEAM/CATCH MODE PROOF:** Side laser entity `80F4B0DC`, graph `80BFC20E`, mode1 node3 sets/rises outputs to1; mode0 node1 leads to node2 and actions `+1AA8/+1B88/+1C68` with cubic `(0,0,-1,1)`, ending at0. Catch entity `80BEEBEA`, graph `8157EB3E`, mode.5 node8->9->10 sets outputs0/1/3 to1 and ramps output2 to1 (`+49E8/+4AF8/+4C08/+4D18`). Mode0 node4->5 sets outputs0/1/3 to0 and ramps output2 to0 (`+4898/+4788/+46B8/+45D8`). Mode1 also fades but enters a separate terminal sequence. Exact catch devices23:93/94 therefore map active to.5, inactive to0; beam devices23:87/89 retain1/0. Raw resources and decoded node/action evidence are retained in the final-puzzle directory.


### Retained hologram creation receipt and confirmed live ending - 2026-09-09

**LIVE READ / USER-REQUESTED REPAIR:** The second process (PID56448, creation FILETIME134334446549791610) used installed DLL `128f8b36264544242b7ebfa9a6cdb8e573362b5d0dee225f5da06ad5e943d34c`. Both held scans progressed automatically with their effective native durations. After row13 / Keep looking, the ending waited on hologram source `59700FA7 / 80B568AF / 4:82`. Its applied generation was2, active1 and creation-committed generation1. The source retained a valid existing entity. Its authority `59700FA7 / 80B5690B / 23:92` resolved the generic component `80FD20CD / 80803910 / +A78`, with both salted handles valid and the generic owned by the source entity. Entity package `80F2625D` references this generic at `+CC`.

The strict observer rejected that retained object because it required the creation counter to equal the new applied generation. The user explicitly requested immediate live continuation. After verifying source, definition, both salted handles and shared entity, one host acknowledgement byte changed00->01. No scan, dialogue, enemy-death or mission-success flag was changed. Native row14 submitted at t1539609; the executor published `mission_finished=1` at t1547672 after the full exchange. The user confirmed closing dialogue, Mission Complete and retained hologram all worked. This is intervention-assisted ending acceptance, not a clean run of the new permanent ownership fix.

**PERMANENT SOURCE:** `hologram_owner.h` allows only the exact hologram source's retained transition from this owner's preparation generation to owner+1, with managed/desired/prepared/active state and the package-linked generic owned by the validated source entity. The native observer revalidates applied/committed generations, active flag and salted entity before publishing the real object receipt. Normal creation-generation matching remains mandatory for every other source. Source-sense9F0750 observes the retained object even when no new creation is needed. The existing hidden-source preparation, persistent display.5 and full closing-dialogue requirement remain.

**OFFLINE COVERAGE:** Exact source scope, prepared/active flags, wrong applied generation, old-lease commit, malformed generic definition/kind/offset, missing self/entity and later ownership leases are tested. Full mission replay still waits for object acknowledgement before coordinates and success. Fresh-process automatic ownership acceptance remains pending.

Evidence: `build/coo/deep-storage-ending-live-20260909/session-review.json`, `ending-before.json`, `native-hologram.json`, `hologram-device-current.json`, `live-ending-repair.json`, and `live-progression.log`. A packed on-disk executable disassembly attempt was rejected and is not evidence for native behavior.

**Prior final-room candidate:** `build/coo/deep-storage-final-room-fixes-20260909/` combines the pit barrier, corrected final plates/beams/rods, real vulnerable-box destruction, and retained hologram ownership. Generated validation/package/installation/acceptance receipts establish delivery after this source freeze. Prior validated payload `build/coo/deep-storage-persistence-final-20260909/payload/` is preserved as a coherent rollback set, in addition to exact pre-install working-file backups.


**LENS DESTRUCTION REVISION:** Accepted native box death reserves a fresh lens-device generation and clears its acknowledgement before changing the published position from.75 to0. Native caller `106AE6D/106AE74` compares the requested revision against generic `+960` and skips the setter unless it is newer. Lua then retires the logical channel after observing actual box destruction; the later trigger-flow correction removed the combat-clearance prerequisite. Regression checks acknowledge.75 first, require a newer0 revision at death, reject stale acknowledgements, and ensure duplicate death cannot advance the generation. Evidence: `build/coo/deep-storage-final-puzzle-20260909/lens-channel-revision-native.txt`.

### User-directed trigger flow and early final-room preload - 2026-09-09

**USER REQUEST:** Load the pictured final-room pillars at the start of the preceding encounter. Stop requiring unrelated enemy deaths for progression and spawning. Keep enemy-death requirements only on portal/drop gates, specifically the Hydra for the warp portal. This supersedes the earlier all-clear policies described in historical acceptance notes.

**SOURCE / PACKAGE / ASSUMPTION:** Reuse recovered polygons with retained crossings. Warp entry uses59700FA7/60:338,340,342; middle uses345/348; Hydra approach uses350/362/363. Deeper conditions also satisfy earlier approach fallbacks. These choices follow source placements and native geometry; exact original graph edges are not claimed. Cyclops stages use209, then211, then212/213, with deeper fallbacks. The pit still requires the real Cyclops and all18 guard-source deaths. Every obsolete death-wait step was removed so whole-graph completion cannot retain an orphan kill dependency.

**PRELOAD:** Cyclops approach requests final source slots72,74,76,77,78,79,80, then their verified initial modes: lens1, side beams1, catches.5, center beam0, covers1, conflux0. Final plate presentation becomes available from its active source while gameplay remains unarmed. Even early occupied plates stay red with stopped timers, no contest sampling and no completion observation. The original timer is followed by pose reconciliation to retain the preload. Arming changes the command identity even if occupancy and revision are unchanged. Entrance behavior is unchanged. Final scan and ending sources retain their original later gates.

**FINAL PUZZLE:** Starter-to-fanatic spawning waits for native readiness rather than starter deaths. First/both plate charges still cue their reinforcement groups. Both plates expose the protected box; authentic destruction alone releases covers/conflux. Optional survivors can remain through the final scan, dialogue and success. Enemy readiness and actual actor ownership remain mandatory for requested spawns; no death or completion is fabricated.

**OFFLINE TEST:** Full-route replay deliberately leaves descent, non-Hydra arena, corridor and final-room enemies alive. It holds the Hydra death to verify portal blocking, covers both Cyclops/guard kill orders, verifies early unarmed red plates during the pit encounter, rejects early charge, and reaches the final scan/success with survivors. Earlier ownership, destruction revisions, scan durations, hologram retention, dialogue and reset tests remain. Focused results: `build/coo/deep-storage-trigger-flow-focused-20260909/`. Fresh native acceptance of this scheduling/preload update is pending.

**DELIVERY:** `build/coo/deep-storage-trigger-flow-20260909/` contains the frozen-source validation, package, patch against the prior installed candidate, installation and rollback receipts. Installed baseline is Release DLL90b25cdf605c340be0ee76b6ddef394c76da8b9195aed6ae2e53487520748398. Prior validated DLL/scripts are preserved together for rollback; settings are not changed.

### Unused descent doorway wall - 2026-09-09

**USER REPORT / LIVE READ:** The user reached the X-shaped hallway barrier and requested that it never be enabled, then explicitly requested immediate live disable. Process47652, creation FILETIME134334532937414371, used validated Release DLL944a6fb6de1befb6c5397a74b05e87b93d91815ce01f029f47ac900bcf6c14f7. The host already desired off at generation1, but the old generic inactive mapping published native0 and raised this wall.

**PACKAGE:** Exact authority E6402111/80B56A48/23:0/+278 uses placement GUID9F62D8B1942FA47D. Placement80ED593F links entity80C3ED80 at(1378.5305,540.0868,-141.8221); generic80C7063B/80803910/+A78. Graph80F4BA2D native0 ramps visual output0->1. Native.2 ramps1->0, changes model event222E5F81 to state54D6B84B, and changes physics event1A8FEB14 to37798A64 after its authored delay. Model80F26F00 and physics80F26F01 require the opposite enabled states. Secondary effect80F26F02 also leaves its active range at.2. Evidence: `build/coo/deep-storage-descent-wall-20260909/research/descent-wall-mode-proof.json` and decoded/raw resources.

**LIVE INTERVENTION / USER CONFIRMED:** Fresh PDB resolution and process creation/hash checks identified the one host channel and its salted native device. Only publication generation1->2 and active0->1 changed to select the installed mapper's.2 branch; logical desired remained off. Native current/target both acknowledged.2 and revision2. The user confirmed the doorway was clear. Combat, scans, plates, dialogue and success state were untouched. Receipt: `live-wall-disable.json`. This repair belongs to that run only.

**PERMANENT SOURCE / OFFLINE TEST:** The mapper now forces.2 for this exact four-part asset identity before the generic inactive return. Even an accidental logical on cannot enable this unused wall. Opening setup requests off before descent; later requests remain idempotent. Tests check neighboring asset isolation, the native147-bit body and literal IEEE7543E4CCCCD position word for both logical inputs, and removal from opening through every mission phase. Focused Debug/Release tests pass. Existing portal and Cyclops drop-barrier behavior is retained.

**DELIVERY:** The wall-only build passed42 checks but was superseded before packaging or installation by `build/coo/deep-storage-door-waves-beam-20260909/`; native proof remains in `build/coo/deep-storage-descent-wall-20260909/`; previous installed rollback is `build/coo/deep-storage-trigger-flow-20260909/payload/`. A running process keeps its currently loaded DLL; installation requires the game to be closed. Generated delivery receipts establish whether the permanent DLL has been installed. Fresh-process permanent removal acceptance remains pending.


### Door requests, independent plate waves and final beam - 2026-09-09

**USER REQUEST:** Request every descent enemy when the Pyramidion door opens. Give the second/left plate an immediate wave on entry and a second wave after clearing the first. Remove the lingering central beam when the box is destroyed. Rebuild and install together with the unused-wall correction.

**SOURCE / CAPABILITY DECISION:** Seventeen exact descent population sources (59700FA7/type1 slots1–17) gain `.request` capabilities with requested completion. The opening graph publishes them in three concurrent batches after its door command; no descent volume or earlier enemy death is required. Existing native population admission, health/AI/tactical readiness and real death tracking remain intact. The existing `.spawn` capability still requires native readiness. Streaming can fulfill the door requests later without holding the player at the entrance. The six descent geometry triggers remain unchanged.

**PACKAGE / RECONSTRUCTED WAVE POLICY:** Recovered source placements divide the final room into odd/left and even/right groups. Left first wave uses slots99,100,101,105,106, followed by109,111,113,114,117. Right first wave uses102,103,104,107,108, followed by110,112,115,116,118. Center simmer98 and dialogue12 run once on first occupation. Each side therefore has five initial actors and seven reinforcement actors; paired Hydra and Minotaur source categories account for the latter count. All21 sources are used once. Side ownership comes from package placements; this two-wave schedule follows the user's correction rather than a claim that the retail scheduler was recovered. A side's second wave waits only on authenticated deaths of its five first-wave sources. Neither wave starts from plate completion, and neither depends on the other plate.

**ENDING / RETIREMENT:** `map.encounter.finish` is a trusted mechanic scoped to the `map_room` domain. Lua requests it only after `map.scan.finished`; the controller advances the phase and cancels any unfinished wave commands. This reuses executor cancellation rather than adding a background scheduler or manufacturing clear receipts. Thus the player's completed final scan can start the ending even with first-wave survivors or delayed reinforcement admission. The per-phase flag resets at transition/reset. Already requested enemies retain their ordinary source ownership.

**BEAM IDENTITY / ROOT CAUSE:** The previous reveal step explicitly requested center `.on` after real box destruction. Exact center authority59700FA7/80B568F9/type23:88/+278 drives source4:76/80B5689C/+4C8 and entity80F4B0CF. Its graph80BFC20E is the same verified beam graph: native1 displays, native0 fades/removes. Actions+1AA8/+1B88/+1C68 take outputs1->0 in the zero branch. Reveal now requests `.off`, preserving the central beam's removed state through conflux scanning, dialogue and success. The conflux and persistent dome remain separate sources.

**OFFLINE VALIDATION / DELIVERY:** Regression work covers all17 door requests while native descent admission is delayed, both plate visitation orders, waves before charge, authentic first-wave clear before reinforcement, survivor completion, beam removal, reset, and all prior wall/pit/scan/hologram checks. Complete results and installation are established by generated receipts under `build/coo/deep-storage-door-waves-beam-20260909/`, not by this frozen source record. Previous coherent rollback is `build/coo/deep-storage-trigger-flow-20260909/payload/`. Fresh native acceptance of these three latest changes is pending; the earlier targeted doorway removal was user-confirmed.
