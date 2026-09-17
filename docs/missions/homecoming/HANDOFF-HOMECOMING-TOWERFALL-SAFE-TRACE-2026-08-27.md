# Homecoming / Towerfall Safe Resolver-Trace Handoff

Date: 2026-08-27  
Workspace: `C:\Destiny 2 Development`  
Target activity: `266 / mission_towerfall`  
Launch donor: `282 / Chosen`

## Executive summary

Homecoming reaches its real `mission_towerfall` opening through the authored Chosen launch path.
The Tower Watch objective and first Ghost dialogue work. The wall-breach animation and first Cabal
wave do not yet run.

The recovered type-43 authority body is structurally correct, but publishing its child reference as
an active Scene entry caused a deterministic native crash. That publication is now disabled behind
an explicit safety gate. Objective, dialogue, mission-director, and script-state publication remain
enabled.

The crash has also been traced past the final fault to the actual failure boundary. Destiny's Scene
entry resolver at `+0x501AD0` returns false. The owning Scene update at `+0xB3F620` then passes a null
runtime source into the common iterator chain. The current build observes the update and resolver,
records the reference and 16-byte output, and never modifies Destiny's result.

The current DLL is safe for ordinary Homecoming and Omega runs. It does not publish Towerfall's
unsafe active Scene entry.

## Current binary

Primary artifact:

```text
artifacts\steam_api64.homecoming-safe-resolver-trace.dll
```

SHA-256:

```text
FDB84886FB64E4BE7E7D0A464108FA35B8C595CF4AA659E5ED265CA96557419A
```

Size:

```text
12,498,432 bytes
```

This same binary was staged to:

```text
C:\Destiny 2 Development\steam_api64.dll
C:\Destiny 2 Development\bin\steam_api64.dll
```

The previous staged DLLs were backed up as:

```text
steam_api64.dll.pre-scene-resolver-trace-20260827-202036.bak
bin\steam_api64.dll.pre-scene-resolver-trace-20260827-202036.bak
```

## Operational status

### Working

- Chosen activity `282` supplies the real native prelaunch route.
- Dawn rewrites the selected activity to `266 / mission_towerfall` before publication.
- Service 6 and matchmaking receive the Towerfall destination contract.
- Homecoming enters bubble `9`, slice set `72`, in the Underwatch/Tower Watch opening.
- The reconstructed roster and all 98 captured Towerfall objects decode.
- Root, global, and local groups are available.
- Type-1 widths `92`, `124`, and `156` decode.
- Identity-1 manager registration and native component dispatch work.
- The opening objective appears.
- Ghost dialogue record 0 plays.
- Dialogue completion advances the authored executor to the breach beat.
- The recovered type-43 body decodes correctly as one entry and zero words.
- The native resolver-failure chain is identified and instrumented.

### Intentionally disabled

- Towerfall's active type-43 breach Scene publication.

The safety gate is:

```cpp
inline constexpr bool kPublishBreachSceneAuthority = false;
```

The breach beat still publishes its directive, dialogue record, mission-director state, and script
state. Its log reports:

```text
scene=0 scene_blocked_unsafe=1
```

### Not working yet

- Wall-breach animation.
- Cabal breaking through the wall.
- First Cabal combat wave.
- Encounter-clear detection in real combat.
- Path unlock after the first encounter.
- A universal package-driven mission cue executor.

## What the crash actually was

The corrected authority packet decoded as:

```text
scene slot   = 0x9D8076E4 / type 43 / index 5
schema       = 0x8080626B
selector     = 0x80B82771
entry count  = 1
entry        = 0x57318E3B / type 2 / index 1
word count   = 0
body width   = 129 bits
```

The body shape is proven:

```text
74 base bits + 55 bits per active entry + 32 bits per word
```

The crash was not a remaining width or alignment defect. The exact native chain recovered from the
minidump and unpacked image is:

```text
+B3F620  Scene component update
  |
  +-- +501AD0  resolve 8-byte Scene entry reference into 16-byte runtime datum
        |
        +-- returns false
              |
              +-- +B3F6CB sets RCX = null
                    |
                    +-- +4E83B0
                          |
                          +-- +4E3410
                                |
                                +-- +A92AE0 adds 0x2F0 to null
                                      |
                                      +-- +A93070 stores 0x314 as iterator pointer
                                            |
                                            +-- +A9309B faults reading [0x314]
```

Crash-time stack returns:

```text
+A92B07
+4E3444
+4E83D2
+B3F6D7
```

At `+B3F620`, the relevant logic is:

1. Check the Scene component's active byte at `component + 0x260`.
2. Build/get the active-entry vector.
3. For each eight-byte entry reference, call `+0x501AD0`.
4. If resolution succeeds, convert the returned datum handle into a runtime pointer.
5. If resolution fails, set the runtime pointer to null.
6. Call the downstream iterator with mode `2`.

The failed resolver result is therefore the real boundary. Skipping `+A9309B` would only hide the
missing binding and leave the Scene partially initialized.

## New observation probe

The active `activity_spawner_chain_probe.cpp` now owns two observation-only hooks:

```text
Scene update:   +0xB3F620
Entry resolver: +0x501AD0
Exact caller:   +0xB3F678
```

Install confirmation:

```text
ev=scene_entry_runtime_probe stage=install result=ok
```

Per-entry result:

```text
ev=scene_entry_runtime_resolve stage=return ...
```

Each result records:

- Native result: `resolved` or `unresolved`.
- Scene component pointer.
- Component value at `+0x25C` and active byte at `+0x260`.
- Entry ordinal in the current Scene update.
- Raw eight-byte reference.
- Parsed registry, type, and index fields.
- Resolver output before and after the call.
- Forced package name.

The probe does not replace a reference, synthesize an output, suppress a failure, or call the
iterator itself.

## Immediate next run

Perform one clean Omega launch with the current DLL. Omega provides a known-good native Scene
resolution timeline.

Look for:

```text
ev=scene_entry_runtime_probe stage=install result=ok
ev=scene_entry_runtime_resolve stage=return ... result=resolved ... forced=mission_scot
```

Preserve the complete log. For each successful entry, compare:

- `reference_raw`
- `reference_registry`
- `reference_type`
- `reference_index`
- `output_after`
- `component_value`
- entry ordering

Then perform a safe Towerfall run. Expected breach handoff behavior is:

```text
ev=tower_watch_executor ... beat=2 ... scene=0 scene_blocked_unsafe=1
```

Towerfall should not crash from the authored Scene packet in this build. Its breach Scene also will
not run yet, because the unsafe publication is deliberately absent.

## Next implementation decision

Use Omega's successful resolver rows to determine what Towerfall lacks before type-43 activation:

1. Compare the successful Omega reference shape and resolver output with Towerfall's
   `0x57318E3B / 2 / 1` reference.
2. Determine whether Towerfall needs a different package reference, an earlier runtime-object
   registration, or a native selector/definition producer to run first.
3. Trace the inner resolver at `+0x4EA140` only if `+0x501AD0` shows that the reference shape itself
   is accepted but the datum lookup fails.
4. Do not re-enable `kPublishBreachSceneAuthority` until the selected Towerfall reference resolves
   to a non-null runtime datum in the same way as Omega.
5. Once the Scene starts safely, correlate its native Cabal spawner requests and then wire the
   encounter-clear/path-unlock beat.

## How to enter Homecoming

1. Ensure Destiny 2 is closed before replacing a DLL.
2. Copy `artifacts\steam_api64.homecoming-safe-resolver-trace.dll` to both:

   ```text
   C:\Destiny 2 Development\steam_api64.dll
   C:\Destiny 2 Development\bin\steam_api64.dll
   ```

3. Start `C:\Destiny 2 Development\destiny2.exe`.
4. Press `Insert` to open the Dawn UI.
5. Open **Activity** and choose **Towerfall opening**.
6. Confirm:

   ```text
   Enabled:   on
   Activity:  mission_towerfall
   Bubble:    9 / 0x81EB50AE / Underwatch
   Slice set: 72 / state 0
   Spawn set: authored/unset
   ```

7. Do not force default spawn set `0x2EA8FB98`; the retained donor route owns arrival placement.
8. In the Director, launch **Chosen**, activity `282`.
9. Dawn uses Chosen only as the prelaunch donor and rewrites the published activity to Towerfall
   `266`.

The override is not persisted across process restarts. Select **Towerfall opening** again after a
restart.

## Important source locations

```text
Dawn\src\middleware\bap\activity_message\tower_watch_cue_manifest.h
Dawn\src\middleware\bap\activity_message\activity_sensor_auth_bodies.cpp
Dawn\src\middleware\bap\activity_message\activity_sensor_auth_encoder.cpp
Dawn\src\middleware\bap\activity_message\sensor_auth_update.h
Dawn\src\server\bap\encrypted\push\activity\activity_roster_snapshot.cpp
Dawn\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp
Dawn\src\client\hooks\bootflow\opening_authority\scene_authority_capture.*
Dawn\src\state\activity\forced\activity_forced_destination.cpp
Dawn\src\server\ui\activity_override\activity_override_panel.cpp
```

Key current lines:

```text
tower_watch_cue_manifest.h: kPublishBreachSceneAuthority = false
activity_roster_snapshot.cpp: scene_blocked_unsafe
activity_spawner_chain_probe.cpp: scene_entry_runtime_probe
activity_spawner_chain_probe.cpp: scene_entry_runtime_resolve
```

## Building

From `C:\Destiny 2 Development` in PowerShell:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
    'Dawn\Dawn.vcxproj' /m /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
```

Output:

```text
C:\Destiny 2 Development\build\x64\Release\steam_api64.dll
```

The current Release build succeeds. An old generated focused-test project still contains absolute
paths to the removed `D:\Dawn-port` tree and must be regenerated before that test project can
run from this workspace.

## Packaged evidence and artifacts

The ZIP includes:

- Current source snapshot and project metadata.
- Current safe resolver-trace DLL.
- Previous pre-active-Scene safe DLL.
- Previous active-Scene crash-repro DLL, clearly labeled.
- Current and prior Dawn logs.
- Corrected and shifted Scene-packet comparison logs.
- Current minidump and crash reports.
- Towerfall extracted scenario, launch, and cue-reference records.
- Generated Towerfall manifest/mapping/edge reports.
- Omega reference logs and prior mission handoffs.
- A concise native resolver-chain note.

It excludes the installed game depot, `packages`, root `bin`, generated build trees, caches, and
other large reproducible data. There is no Git metadata in this workspace, so this ZIP is the
authoritative source snapshot for this handoff.

