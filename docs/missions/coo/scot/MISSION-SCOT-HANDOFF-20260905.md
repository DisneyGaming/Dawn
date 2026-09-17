# Mission Scot / Omega handoff — 5 September 2026

## Read this first

**The working Infinite Forest route is preserved. The Panoptes spawn repair is installed and awaiting the user's gameplay result. A separate candidate adds native Forest Vex selection and bounded diagnostics; that candidate is built and tested offline but is NOT installed.**

The user's explicit instruction is: **“dont launch it, let me launch the cmd file. it breaks when u launch it.”** The assistant must not start Destiny, invoke its launcher, or restart it. The user controls launching through [launch-scot-reveal-debug.cmd](</C:/Destiny 2 Development/launch-scot-reveal-debug.cmd>). This CMD uses the root installed DLL; it does not install a candidate.

The latest work was to implement the supplied native Forest enemy handoff while the user tests Panoptes. No game launch, DLL replacement, cache cleanup, or settings change was performed during that Forest implementation. This handoff request also made no runtime changes.

## Workspace and evidence

- Active workspace/game directory: `C:\Destiny 2 Development`.
- Installed DLL: `C:\Destiny 2 Development\steam_api64.dll`.
- Active settings: `C:\Destiny 2 Development\Dawn\settings.json`.
- Actual cache: `C:\Destiny 2 Development\Dawn\cache`.
- Game log: `C:\Destiny 2 Development\Dawn\logs\dawn.log`. Archive it before a later launch can replace it.
- Forest implementation archive: [build/scot-forest-enemies-20260905](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905>).
- Panoptes repair archive: [build/scot-boss-spawn-fix-20260905](</C:/Destiny 2 Development/build/scot-boss-spawn-fix-20260905>).
- Supplied enemy document, preserved unchanged: [HANDOFF.md](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/HANDOFF.md>).

This workspace has no Git repository metadata or `Dawn.sln`. The supplied document's `D:\Dawn-port`, `D:\Dawn-work`, and `D:\Destiny3\bin\x64` paths are not available here. Treat their referenced research as external handoff evidence, not as files verified locally. Do not apply D: deployment commands to this C: installation.

## Installed build versus staged build

### Installed: Panoptes spawn repair

Root DLL SHA-256, rechecked when preparing this handoff:

```text
52A03B8175341740F1EB48F2AC78122734077519CDC8F1AF2B4CCF0924F748BE
```

This matches [run-manifest.json](</C:/Destiny 2 Development/build/scot-boss-spawn-fix-20260905/run-manifest.json>). Startup verification passed with reveal revision 3 and the native request entry verified. Actual Panoptes appearance and the intro remain unconfirmed; wait for the user's run result and inspect that run's log. Do not infer success from compilation or startup installation alone.

### Staged: combined Panoptes repair plus Forest enemies candidate

Candidate root:

```text
C:\Destiny 2 Development\build\scot-forest-enemies-20260905\candidate-20260905-030623
```

DLL: [out/steam_api64.dll](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/candidate-20260905-030623/out/steam_api64.dll>).

```text
DLL SHA-256:
31C8585FDC17D55DF14A5B1926C14F4E84996561CC3DEE5F3B94F89CB037F0C4

Source manifest SHA-256:
1512D4AACE09003E0593A49C4D878077A24077EC2F9F40B04BB3F9B5FFF36B8D

Embedded build ID:
DBFD1FD8B7F9AEC6A3555A349B7CB62D00B2C47C4E96EFDC691A77588D813B49

Active settings SHA-256 at freeze and handoff:
B2C0AD3655561F85EC08D3D23D3BBBC53249B85F4BBE7A09D329D6BA6FEAA8D4
```

The candidate's [candidate-manifest.json](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/candidate-20260905-030623/candidate-manifest.json>) also records symbols, test logs, build log and generated-header hashes. Its status is `built and verified; not installed`. Do not confuse a newly built DLL under a build directory with the root DLL loaded by the launcher.

## What already works, and what remains unresolved

The user confirmed the corrected Forest generation worked. It connects to the entrance stairs, supports the single route to the fixed exit, and the tunnel to Infinity's Crown works. Preserve this behavior while adding combat.

Ghost dialogue was advanced and the user confirmed the Forest line works after its trigger was moved earlier. Objective markers/checkpoints still failed to retire or advance consistently: an earlier entrance marker could reappear after moving away. The user explicitly deprioritized that issue to focus on Panoptes. Do not describe checkpoints as fixed.

Panoptes previously failed to spawn despite reaching the reveal area. The installed repair addresses two concrete request-path bugs described below. There is no confirmed successful gameplay result for this latest repair in the handoff.

The new Forest candidate supplies the missing race selection. Live Vex reception, authored enemy spawning, AI, death accounting, and red-to-white gate behavior remain to be demonstrated. This is not yet finished combat support. Lair enemy-wave activation and category counts are separate unresolved work; a summon animation does not establish wave activation.

## Preserve the working Forest recipe

Scope is exactly `mission_scot`, definition class `80804FEC`, piece set `80F4E6E7`; Forest-D container is `80F4E6E8`.

- `g0 +X`: column -1, height 2, weight 0, initially closed.
- `g1 -X`: column -1, height 0, weight 0, initially closed.
- `g2 +Y` entrance: column 2, height 0, weight 0, initially open.
- `g3 -Y` exit: column 1, height 2, weight 0, initially closed.
- Actual native solver inputs are `0, 0`, scoped to this worker, through the hook at RVA `FF2F80`.
- One positive seed is chosen per mission run and retained across region transitions and worker reconstruction. Orbit/new run advances the run generation. A repeated first prefab is possible because the palette is finite.

Column -1 removes an endpoint; closing its gate alone still allows a route to it. Anchor packing is not a uniform nine-byte stride. The recipe writes fallback anchors at worker `+970` and seed mask/value at `+948/+94C`; effective recipe, generated entries and door progress remain native-owned. Worker `+950/+954` can still show presets even when the actual solver call receives zeros.

The verified native owner-authority repair remains in place. Sensor self-authority and worker-owner authority are different checks. The earlier host generator authority body remains deliberately disabled (`kOmegaForestGeneratorBodyReady=false`) because its unproven encoding blocked native seeding; the enemy patch does not enable it.

Sources: [omega_forest_recipe.h](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_forest_recipe.h>) and [omega_dialogue_dispatch_probe.cpp](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp>).

## Forest enemy implementation

`Snapshot::omegaForestVexEncounters` is true for exactly `mission_scot` from the opening snapshot onward. It is independent of region and portal/presentation quiescence. Other missions retain the 520-bit lifetime body. Selection must arrive before piece generation; late changes are not proven to repopulate existing pieces.

Type-17 lifetime now carries two typed switches:

- Preserved waiting selector: key `B3C1251B`, value class `80800007`, decoded signed integer 0, wire value `80000000`.
- Added Vex selection: key `67AF9045`, value class `80800070`, raw hash `050C5D2E`.

The added entry is `key[32] + present[1] + class[32] + value[32]`, adding 97 bits. The list count changes from one to two; declared and written body widths both become **617 bits**. The Vex value has no integer bias. Boolean true and wildcard `050C5D35` are not substitutes. Spawn overrides, quarantine fields and the configuration tail are preserved.

The intended native path is activity lifetime publication, classification, per-piece authored selection, native actor requests, encounter completion, then gateway unlock and player interaction. Native population and AI remain responsible for combat. No HUD Spawner calls, arbitrary enemy counts, replacement kill counter, forced gate state/color, actor AI patch, or forced future-island activation was added.

The supplied document reports 5,936 available actor rows across 26 Forest palettes. That is available authored content, not a population to spawn. Vex-only selection is the supported first implementation, not a recovered complete retail faction policy.

Changed production sources:

- [omega_forest_encounters.h](</C:/Destiny 2 Development/Dawn/src/state/activity/omega/omega_forest_encounters.h>): exact mission scope and switch constants.
- [sensor_auth_update.h](</C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/sensor_auth_update.h>): snapshot flag.
- [activity_roster_snapshot.cpp](</C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp>): stable mission-scoped selection.
- [activity_sensor_auth_bodies.cpp](</C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp>): typed entry and correct width.
- [omega_enemy_forest_receipts.h](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_enemy_forest_receipts.h>): bounded population decoder and validation.
- [omega_enemy_forest_receipts_runtime.h](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_enemy_forest_receipts_runtime.h>): read-only native observation adapter.
- `omega_dialogue_dispatch_probe.cpp`: invokes that adapter after the exact Omega worker's native tick.

The complete patch and original modified files are preserved under [changes.patch](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/changes.patch>) and the archive's `before/` directory.

## Interpret the Forest logs correctly

New prefix: `ev=forest_enemy`. These receipts require the new candidate; their absence from the installed Panoptes-only build is expected. No separate runtime flag is required for the new receipts.

- `stage=selection vex=1/050C5D2E` confirms the selected Vex pair reached the worker. It does not prove enemies exist.
- `stage=entry` reports state, palette, raw area identity, `native_23`, onward `allowed`, pending byte, encounter handle, and owner-authority read status. State 4 is the native active request path. `native_23` is deliberately uninterpreted. `owner_known=0` means the authority observation failed or was unavailable.
- `stage=population status=3` means the encounter/backlinks and all reported rows validated. Enabled rows contribute `remaining` and `queued`. `tracked_actor_refs` is a reference count, not a verified living-enemy count.
- Population status 1 means lookup/read failure; 2 means header read but later validation/read failure; 4 means unavailable native getter prefix. None means zero enemies. Unknown counters remain -1.
- `stage=gateway` reports member count and uninterpreted `native_354`; material remains `unverified`. It is not a red/white color measurement.
- `invalid_worker`, `capture_limited`, or `budget_exhausted` means observation is incomplete. Missing or failed observations are not zero-population results.

Limits are four recently active workers, first 256 entries/gateways, at most two polls per second globally, 128 KiB copied data and four verified existing-reference getter calls per poll, and 1,024 log lines per mission run. Getter-internal reads fall outside the copied-data budget. It is a bounded change log, not a full frame trace. Only the current tick's supplied worker is dereferenced; cached worker addresses identify observation slots.

Relevant native anchors on the pinned executable are publisher `DBC710 -> A55B20 -> A55110`, worker selected pairs `+9C0`, encounter getter `4F0290`, authored counts `FEF750`, active requests `1007350`, completion/unlock `10059A0 -> 10020A0 -> DF67F0`, and onward permission `10058C0`. The local adapter accounts for the +0x10 data prefix after relative-array anchors. See archived `native-*.txt` disassemblies and [verification.json](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/verification.json>) for the reviewed image/code identities. The companion `native-*.exe` files are offline disassembly wrappers and must not be launched as a game or test tool.

## Installed Panoptes repair and intro boundary

The archived failing run reached reveal route 4, bubble 14, with the native boss spawner loaded/seeded but no boss request. Two defects blocked requests:

1. The request helper depended on `g_requestOriginal` from the retired spawner debug probe, whose hook was disabled.
2. A `safe_read<int64>(component, 8)` call supplied 8 as a fallback instead of reading component +8, so its source check rejected the component.

The installed repair verifies native request RVA `4E2E80` and calls it directly for source `80F4756A`, class `80809A3B`, resource offset `+728`. That offset identifies the runtime source resource; it is not the on-disk relative pointer stored at definition byte +8. Native code retains category selection, `sr_boss_location_1`, construction and queue processing. Broad spawner debugging remains disabled.

Requests are bounded, scoped to the loaded reveal area, avoid duplicating pending/deferred requests, and stop after acceptance. Native live accounting uses `4E3410`. Inspect:

```text
ev=omega_reveal stage=install result=ok revision=3 ... spawn_request=verified_native_4E2E80
ev=omega_reveal stage=boss_component ... route=4 area=14 eligible=1
ev=omega_reveal stage=boss_request ...
ev=omega_reveal stage=boss_live ... count=1
ev=omega_reveal stage=boss_wait ...
```

An accepted/queued request is not proof of visible Panoptes. `boss_wait` distinguishes pending native work from acceptance without a live actor. Correlate native live count with the user's view.

The existing intro path waits for observed boss existence, then targets component `80F478D3`, cinematic hash `A74B2200`, catalog lookup `C4C1A0`, and native start `1069CC0`. `intro_wait`, `intro_request`, `intro_already_playing`, and `intro_stopped` provide separate evidence. Neither intro acceptance nor summon animation proves Lair enemy-wave activation.

Sources: [omega_boss_spawn.h](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_boss_spawn.h>) and [omega_reveal_native.cpp](</C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_reveal_native.cpp>). Prior repair details and evidence: [Panoptes RUN.md](</C:/Destiny 2 Development/build/scot-boss-spawn-fix-20260905/RUN.md>).

## Tests and build provenance

Eight test runs passed: Debug and Release for `omega_progression_tests`, `omega_forest_recipe_tests`, `omega_forest_roster_tests`, and `omega_experiment_settings_tests`. They cover exact packet width/typed values, full 11-group framing, unchanged tails with/without spawn overrides, short-buffer rejection, scope/quiescence, recipe/seed lifecycle, native population read failures/bounds/budgets, and existing presentation, teleport acknowledgement, boss gateway and settings behavior.

Debug exposed an existing roster test's raw `memcmp` over struct padding. The test now compares every `Definition` field; production roster logic was not changed for that failure. Canonical passing logs are named `omega_*_tests-Debug-run.log` and `omega_*_tests-Release-run.log`. The shorter `roster-Debug-run.log` preserves the earlier padding-test failure.

[verify_evidence.py](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/verify_evidence.py>) independently decoded the actual exported 617-bit packet, checked both typed entries and the entire tail/padding, verified identical Debug/Release exports and the pinned getter prefix. The external handoff's 48-case native predicate/publication harness is unavailable locally and was **not rerun**. Local offline checks do not prove live enemy behavior.

The candidate was built from 1,260 frozen source/project/resource/test files with MSBuild Release x64, v145, MSVC 14.51.36231, SDK 10.0.26100.0. The Release build passed with zero warnings/errors. Source copies matched the workspace before and after freezing and remained unchanged after build. The DLL contains both new receipt strings and the generated build identity.

The original frozen-candidate helper requires Git metadata and a solution absent here. [freeze_candidate.ps1](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/freeze_candidate.ps1>) instead uses the repository's canonical source-manifest/identity functions on an explicit input set. Frozen source is read-only; output and intermediates are outside that snapshot. Zero Git HEAD and `workspace-no-git` are explicit metadata sentinels. Source hash excludes the generated identity header, which has its own recorded hash. A single build is not a reproducibility demonstration.

## Immediate next steps

1. Let the user finish the installed Panoptes test and launch the CMD themselves. Preserve the resulting log before changing builds. Check revision 3, loaded reveal area, request outcome, native live count and the user's visible result. Investigate the first missing stage; do not guess new Lair wave counts.
2. When ready to switch to the Forest candidate, keep Destiny closed and use [install-forest-candidate.ps1](</C:/Destiny 2 Development/build/scot-forest-enemies-20260905/install-forest-candidate.ps1>). It is prepared and syntax-checked but **has not been executed**. It verifies candidate and expected installed hashes, backs up DLL/settings/log/cache, clears only direct regular files in the validated actual cache directory, installs the candidate, and verifies DLL/settings hashes. It never closes or launches the game. If the installed DLL changed meanwhile, it stops for review.
3. The user launches the same CMD and starts a fresh Omega run. Verify the installed candidate hash/startup build identity first; selection must precede piece generation. Leave Insert force-entry, seed, AI and arbitrary Spawner overrides untouched for the combat test.
4. At the first generated island, capture selection and population before fighting. Observe enemies and appropriate native behavior; check that the forward gate remains red/unusable while combat is outstanding. Kill the island's enemies, observe native completion and a white/usable gate, then hold E and verify exactly one next island becomes available. Continue to the fixed exit.
5. Archive the full log and the user's observations. If necessary, trace the first failed native stage using the diagnostics below. Keep the route and unrelated Panoptes behavior intact.

Failure triage:

- No Vex pair: verify actual installed build and lifetime publication before generation.
- Pair present, no enabled rows: inspect selected palette, classification and predicate ancestry.
- Enabled rows/remaining requests but no enemies: inspect entry state 4, owner authority, active caps, queued requests and native actor creation results.
- Enemies exist but do not behave: inspect native actor authority, navigation, perception and authored AI state.
- Gate opens with enemies remaining: inspect authored completion groups, pending and clear transition.
- All visible enemies dead but gate remains locked: inspect outstanding/queued requests, tracked actors and completion groups, including actors outside the visible area.

Keep failed reads distinct from valid zero counts. Preserve normal native encounter completion; do not hide missing population with artificial gate or color changes. Checkpoint retirement, complete Lair combat progression, and successful Panoptes cutscene playback remain separate unfinished items.
