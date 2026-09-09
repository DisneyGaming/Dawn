# Mission Scot: Panoptes spawn and cutscene handoff

**Date: 5 September 2026. Workspace: `C:\Destiny 2 Development`.**

## Current task and confirmed baseline

The user confirmed: **“enemies spawn and activity progresses.”** Infinite Forest generation, enemy selection and activity progression now work. Preserve that implementation. The next task is **Panoptes spawning and the authored reveal cutscene at Infinity’s Crown**. Objective marker/checkpoint issues were previously deprioritized; do not turn this into another marker rewrite.

The user has now closed Destiny. **Compile, verify, deploy and prepare the CMD launcher as needed, but never launch the game for the user.** The user explicitly wants to double-click the CMD themselves. Do not stop at a staged DLL when asked to implement a patch: complete deployment and verify the installed hash while the game is closed.

This handoff supersedes the older `MISSION-SCOT-HANDOFF-20260905.md` statements that Forest enemies are untested or that the enemy build is only staged. The user's confirmation establishes spawning and progression; it does not separately establish every AI behavior, gate material mapping or Lair wave mechanic.

## Installed build

- Game launcher: `C:\Destiny 2 Development\launch-scot-reveal-debug.cmd`.
- Installed DLL: `C:\Destiny 2 Development\steam_api64.dll`.
- DLL SHA-256: `3E16D552DE382266A7C2233CB75DA3D07441A6BD51CE700379AE5D11C635511D`.
- Build ID: `63B839B1D4D696E04EEE9834FD66A385AC7EDDAA1D7C9B7D3D84EE70CB53F780`.
- Source SHA-256: `896589A89B32C0747EC16C215E082A17FE431A1BF56AB6C725A2DCC71CF4F034`.
- Frozen candidate: `C:\Destiny 2 Development\build\scot-forest-enemies-20260905\candidate-20260905-031913`.
- That directory contains `source`, `out/steam_api64.dll`, matching PDB, `candidate-manifest.json`, `evidence/source-manifest.tsv` and `evidence/release-build.log`.
- Deployment receipt: `C:\Destiny 2 Development\build\scot-forest-crash-20260905\installed.json`.
- Active settings: `C:\Destiny 2 Development\Sunrise\settings.json`.
- Actual cache: `C:\Destiny 2 Development\Sunrise\cache`.
- Current log: `C:\Destiny 2 Development\Sunrise\logs\sunrise.log`.

The current DLL combines the Panoptes request repair, working Forest recipe, Vex selection, bounded Forest receipts, and startup crash repair. Release built with zero warnings/errors; ten Debug/Release regression runs passed. Settings were preserved and generated cache files were backed up/cleared at installation. The user subsequently launched and confirmed Forest enemies/progression.

## Latest evidence: distinguish what was and was not tested

A stable log snapshot was archived while that run was still open:

`C:\Destiny 2 Development\build\scot-panoptes-handoff-20260905\sunrise.snapshot-20260905-032906.log`

Machine-readable summary: `C:\Destiny 2 Development\build\scot-panoptes-handoff-20260905\evidence.json`.

That snapshot proves:

- The exact installed hash/build ID above was loaded.
- `omega_dialogue ... result=ok ... revision=5 transaction=atomic hooks=20`.
- `omega_reveal ... result=ok revision=3 ... spawn_request=verified_native_4E2E80`.
- Forest workers received `vex=1/050C5D2E`, with seed `1436510523` in that run.
- Native population receipts appeared and the user independently confirmed enemies/progression.
- No assert hits were captured.

It contains **zero `boss_request`, zero `boss_live`, and zero `intro_*` events**. Its last logged route is `forest_exit`. That route names the current navigation target: it can become active on entering the Forest and does **not** prove the player reached the physical exit. This snapshot therefore does not establish a new Panoptes spawn failure at the reveal area. **Read the now-closed run's final current log first**, because it may contain events after this earlier snapshot. Archive that final log before another launch replaces it.

Forest receipt logging reached its 1,024-line run budget. Later missing Forest receipts do not mean generation or population stopped. `tracked_actor_refs` is not a verified living-enemy count; the snapshot frequently reports 255, so do not infer enemy totals from it.

## Immediate next step

1. Read/archive the final current log. Verify the loaded build and both successful installation events.
2. Look for progression to `route=reveal`, native loaded bubble `14`, and the exact boss component below.
3. Find the first missing stage: component discovery, eligibility, native request, native live actor, cinematic registration, or cinematic start/playback.
4. If the run never reached reveal, use the already installed build for the user's next manual test through the working Forest/tunnel. Do not invent another spawn patch from the absence of pre-reveal events.
5. If a stage is missing at reveal, add bounded, mission/source-scoped receipts or fix the demonstrated defect. Build, test and deploy the resulting candidate; leave the user to launch the CMD.

## Panoptes implementation already present

Primary files:

- `Sunrise/src/client/hooks/bootflow/omega_reveal_native.cpp`: boss observer/request, cutscene trigger, and existing waypoint integration.
- `Sunrise/src/client/hooks/bootflow/omega_boss_spawn.h`: exact source filter, reveal eligibility and request ABI.
- `Sunrise/src/client/hooks/bootflow/omega_reveal_bindings.h`: 14 pinned native entry signatures.
- `Sunrise/src/state/activity/omega/omega_progression.h` and `.cpp`: run-local route and loaded-area state.
- `Sunrise/src/server/bap/encrypted/push/activity/activity_roster_snapshot.cpp`: mission roster publication.
- `Sunrise/unit/omega_progression_tests.cpp`: boss gateway and existing mission/wire regressions.

All paths in this section are relative to the workspace above.

### Exact boss identity and native request

- Lair bubble: **14**.
- Boss registry: **`95FB2E01`**; squad slot **type 1 / index 0**, authored name **`sq_boss`**.
- Runtime component header: resource tag **`80F4756A`**, definition class **`80809A3B`**, resource-relative offset **`+728`** stored at component **`+8`**.
- Panoptes actor definition: **`80F45253`**.
- Authored placement: **`sr_boss_location_1`**, reference **`95FB2E01 / 66 / 57`**.
- Regular spawner update hook: RVA **`4EDC20`**.
- Verified native request entry: RVA **`4E2E80`**.
- Native live-count helper: RVA **`4E3410`**.

The request gateway calls the verified native function directly. Its current ABI is:

```cpp
using Request = void(*)(std::byte*, std::uint64_t,
                       const std::int32_t*, std::int32_t*) noexcept;
// reason = 2; ten-int request array {1, 1}; nine-int result array.
```

The one authored category was recovered from the source asset. The two leading ones are the existing request encoding, not an instruction to spawn two bosses or guess Lair wave counts.

`spawner_tick()` first forwards the native update, then requires active forced `mission_scot`, an arrived world, the exact component identity, `Route::reveal` (numeric 4), and `loadedBubble == 14`.

It queries native live counts. A positive first category logs `boss_live`, latches `bossSeen` and its time, and suppresses further requests. It avoids requests while native pending/deferred work exists; otherwise it makes at most five attempts at two-second intervals. Current acceptance logic is `result[0] > 0` and the status byte at result offset +4 is not 1, based on the native consumer at `4E311A`. Acceptance is a queue observation, not proof Panoptes exists. Accepted requests are not repeated across region reloads in the same mission run.

Useful component fields in current logs:

- `+260`: initialized byte.
- `+268`: pending count.
- `+650`: desired count.
- `+670`: deferred count.

Expected events:

```text
omega_reveal stage=boss_component ... route=4 area=14 eligible=1
omega_reveal stage=boss_request ... entries=... status=... queued=...
omega_reveal stage=boss_live ... count=1
omega_reveal stage=boss_wait reason=native_request_pending ...
omega_reveal stage=boss_wait reason=accepted_request_without_live_actor ...
```

### Two earlier request bugs are already fixed

The old helper depended on `g_requestOriginal` from a disabled broad spawner debug probe, so it never called native code. It also used `safe_read<int64>(component, 8)` with 8 as a fallback, rather than reading component +8, and rejected the source.

Do not restore either path or enable `kEnableActivitySpawnerChainProbe`. The exact runtime resource offset +728 is **not** the relative pointer value found at byte +8 inside the on-disk definition.

Evidence/archive:

- `build/scot-boss-spawn-fix-20260905/RUN.md`
- `build/scot-boss-spawn-fix-20260905/native-verification.json`
- `build/scot-boss-spawn-fix-20260905/verify_native.py`
- `build/scot-boss-spawn-fix-20260905/sunrise.no-boss.log`
- `build/scot-reveal-20260905/reveal-groups.json`
- `build/scot-reveal-20260905/80F4756A.bin`

The old failed log reached reveal with the spawner loaded but no request. The offline checks proved source/category/placement and reproduced the old blockers. They did not prove the repaired request produced a visible boss.

## Cutscene implementation and unresolved ordering

Exact authored component:

- Registry **`F4D0E0B2`**, object **`80F47979`**, bubble **14**.
- Slot **type 6 / index 22**.
- Name **`boss_reveal.boss_intro_cinematic._cinematic`**.
- Resource **`80F478D3`**, class **`80804F06`**, runtime source offset **`+2E8`**.
- Authority schema **`80804F08`**.
- Cinematic hash **`A74B2200`**.
- Native component tick **`106AB20`**.
- Catalog lookup **`C4C1A0`**, used by native **`C4EA60`**.
- Native component start **`1069CC0`**.
- Native playing byte **component +260**.

Current code requires reveal/bubble 14 and a previously observed live boss. After 500 ms it checks that the cinematic hash is registered, then calls the native start helper. Start attempts are capped at five, two seconds apart. Missing registration logs `intro_wait`; it does not call start on a null catalog lookup. Already-playing and later stopped states are logged separately.

```text
omega_reveal stage=intro_wait ... reason=cinematic_not_registered
omega_reveal stage=intro_request ... accepted=... native_playing=...
omega_reveal stage=intro_already_playing ...
omega_reveal stage=intro_stopped ...
```

Successful return or a playing flag is not proof that the correct camera, actor bindings and reveal animation appeared. User observation is still required. The current boss-before-cutscene ordering is an implementation choice awaiting live confirmation; if native evidence shows the authored sequence owns boss creation, investigate that dependency rather than forcing flags.

The supplied inventory found no standalone package record for `A74B2200` and inferred an in-engine sequence. Treat that as research context, **not proof the catalog lookup can never succeed**. If registration is missing, trace the owning sequence/container activation and native registration path. Do not bypass the lookup blindly.

Relevant assets/research: `build/scot-reveal-20260905/80F478D3.bin`, `reveal-groups.json`, its `native-*.txt` disassemblies, and `C:\Users\gauta\Downloads\MISSION-SCOT-INVENTORY.md`. Companion `native-*.exe` files are disassembly wrappers, not launchers.

## Reveal gating: likely checks before changing spawn code

`Route::reveal` is latched after reaching within 18 game units of `kCrownEntrance = (-1491.7739, 415.2429, -26.9186)`, after the prior route stages. Loading bubble 14 advances the route at least to `crownEntrance`; loading alone does not request Panoptes.

The native loaded-bubble value is currently refreshed through `omega_reveal_native::update_directive()`, which recognizes the exact directive component `80F47BD4 + B88`, calls native area/context helpers and then `note_native_area()`. Thus the boss gate can remain stale if that component/update path is absent, even if the player visually reaches the Lair. If `eligible=0`, inspect the real native area and route first; do not simply force route 4.

If `boss_component` never appears despite a successful reveal install, its current log is after the exact source filter. Absence does not distinguish an absent spawner from a mismatched runtime identity. Add bounded discovery receipts for actual runtime headers/roster binding before choosing a different identity.

The separate reveal owner still installs its two hooks sequentially and directly stores originals. It has not been converted to the main bundle's atomic/wait-publish pattern. Also, the main bundle ignores its install return value; main-bundle success alone does not prove reveal installation. The latest archived run has the explicit reveal success event, so these are code-review concerns, not established causes of the current spawn issue. `cinematic_tick()` also reads its identity before its full readability check; harden that ordering when editing this owner.

## Preserve the working Forest and startup repair

Do not change these while working on Panoptes:

- Forest-D container `80F4E6E8`, piece set `80F4E6E7`, worker definition `80804FEC`.
- Entrance: column 2, height 0, initially open. Exit: column 1, height 2, initially closed.
- Side columns -1; endpoint weights zero; actual scoped solver inputs 0,0 through `FF2F80`.
- Positive seed per mission run, stable across region changes/reconstruction.
- Existing native worker-owner authority repair.
- Host generator body remains disabled (`kOmegaForestGeneratorBodyReady=false`).
- Type-17 waiting switch `B3C1251B / 80800007 / 80000000` plus Vex `67AF9045 / 80800070 / 050C5D2E`; exactly 617 bits for Omega, 520 for other missions. Preserve all tails and overrides.
- Native encounter creation, AI, death accounting and gateway progression. No HUD Spawner, arbitrary counts, replacement kill counter, forced gate colors/states or actor AI patches.

The last Forest loading crash followed `omega_dialogue ... reason=attach`, then a forced tunnel hop with the teardown guards missing, then native index-heap double-free. The repaired main bundle installs all 20 hooks atomically with three bounded attempts, waits for original-pointer publication, and blocks its transport if support is unavailable. Current successful startup is revision 5. Keep that repair.

Details: `build/scot-forest-crash-20260905/RUN.md`, `changes.patch`, crash/previous logs; production `omega_dialogue_dispatch_probe.cpp`, `client/hooking/detour.cpp/.h`; tests `omega_hook_bundle_tests.cpp`.

Forest progression and Lair wave activation are separate systems. Lair template variants do not establish enemy/wave counts. Panoptes summon/reveal success alone will not complete the later boss fight or proxy waves.

## Build, verify, deploy; user launches

This workspace has no Git metadata or `Sunrise.sln`. Build `Sunrise/Sunrise.vcxproj` directly with the installed toolchain:

```powershell
& 'C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe' `
  'Sunrise/Sunrise.vcxproj' /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Affected unit projects support Debug and Release: `omega_progression_tests`, `omega_forest_recipe_tests`, `omega_forest_roster_tests`, `omega_experiment_settings_tests`, and `omega_hook_bundle_tests`, under `Sunrise/unit`. The hook test runs against `destiny2_unpacked.bin` in its own process and executes no game code. The other four tests take no arguments; progression optionally exports the lifetime body.

For a final candidate, use a new frozen source/output directory and preserve manifests, build log, DLL/PDB and hashes. The local `build/scot-forest-enemies-20260905/freeze_candidate.ps1` reuses canonical manifest/identity helpers without Git; inspect/adapt its companion finalization scripts for the new change. Do not edit the existing read-only frozen source.

Before deployment, verify Destiny is closed. Back up the installed DLL, active settings and final log. Clear only direct generated files in the verified actual `Sunrise/cache`, preserving backups/hashes. Deploy the new DLL to the workspace root and verify its hash against the candidate. Preserve settings; do not replay an installer with an obsolete expected DLL hash. The old candidate installer now records an installed candidate and is not a general future installer.

**Leave the game closed. Tell the user the build is ready and link `launch-scot-reveal-debug.cmd`. Never launch it on their behalf.** Re-query PID/module base for any later live inspection; addresses and PID 54844 in the archived snapshot belong to a closed run and must not be reused.
