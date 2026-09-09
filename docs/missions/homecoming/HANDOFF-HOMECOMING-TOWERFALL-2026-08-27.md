# Homecoming / Towerfall Mission Reconstruction Handoff

Date: 2026-08-27  
Workspace: `C:\Destiny 2 Development`  
Target activity: `266 / mission_towerfall`  
Launch donor: `282 / Chosen`

## Executive summary

Homecoming now launches into its real `mission_towerfall` Underwatch opening through Destiny's authored Chosen prelaunch path. The reconstructed activity roster, identity-1 manager binding, objective presentation, and first Ghost dialogue work. The next beat does not yet work: publishing the recovered wall-breach Scene state reaches Destiny's native Scene consumer but deterministically crashes because the recovered package reference does not have a valid bound runtime context.

The latest run proves that the Scene authority encoder is structurally correct. Destiny decoded the full 129-bit body as:

```text
selector    = 0x80B82771
entry_count = 1
entry       = 0x57318E3B / type 2 / index 1
word_count  = 0
mode        = 0
```

Destiny then crashes at `destiny2.exe+0xA9309B`, where the Scene consumer executes `cmp eax, dword ptr [rcx]` with `RCX=0x314`. This is no longer a packet-width or bit-alignment problem. The `0x57318E3B/2/1` row is package metadata or a child anchor that still requires native runtime binding; it is not safe to publish directly as an active Scene entry.

## Current operational status

### Working

- The UI exposes a measured **Towerfall opening** profile.
- Chosen activity `282` supplies the authored launch contract.
- The launch record is rewritten to activity `266 / mission_towerfall` before native publication.
- Service 6 and matchmaking receive the Towerfall destination contract.
- Homecoming reaches `activity:in_world` in bubble `9`, slice set `72`.
- The reconstructed roster delivers and the captured 98 Towerfall objects decode.
- Root, global, and local groups are available to the mission runtime.
- Type-1 sense bodies with widths `92`, `124`, and `156` are decoded.
- Identity `1` manager registration and native component dispatch are established.
- The opening objective appears.
- Ghost dialogue record 0 plays and its completion advances the authored executor.
- The first three authored cue keys are known:
  - opening: `0x4FCECAB6`
  - breach: `0x432D2C95`
  - path unlock: `0x432D2C96`

### Not working

- The wall does not break.
- The first Cabal wave does not spawn.
- Encounter-clear detection and path unlock have not been exercised because the encounter never starts.
- Publishing the active type-43 Scene entry currently crashes Destiny.
- A mission-independent cue-graph executor is not complete; the current first-three-beat flow is an authored Towerfall implementation built on generic roster/schema infrastructure.

## How to enter Homecoming

Use the safe pre-active-Scene DLL described below. The current diagnostic DLL enters Homecoming but crashes when the first Ghost line hands off to the wall-breach Scene.

1. Close Destiny 2 before replacing `steam_api64.dll`.
2. Copy `artifacts\steam_api64.homecoming-safe-no-active-scene.dll` to both locations:

   ```text
   C:\Destiny 2 Development\steam_api64.dll
   C:\Destiny 2 Development\bin\steam_api64.dll
   ```

3. Start `C:\Destiny 2 Development\destiny2.exe`.
4. Press `Insert` to open the Sunrise UI.
5. Open **Activity** and find **Activity override**.
6. Click **Towerfall opening**. This is preferred over manually selecting each row.
7. Confirm the resulting profile:

   ```text
   Enabled:   on
   Activity:  mission_towerfall
   Bubble:    9 / 0x81EB50AE / Underwatch
   Slice set: 72 / state 0
   Spawn set: authored/unset
   ```

   Do not force the default `0x2EA8FB98` spawn set for this profile. The retained Chosen route owns the actual arrival point.

8. Return to the Director and launch the **Chosen** activity, activity index `282`. Chosen is only the donor launch entry; Sunrise replaces its selected contract with Homecoming activity `266` before publication.
9. Expected confirmation lines in `Sunrise\logs\sunrise.log` include:

   ```text
   profile=towerfall ... commit=awaiting_chosen_activity_282
   destination=mission_towerfall trigger=authored_chosen_prelaunch_282
   source=282 destination=266 package=mission_towerfall
   change-world: loaded mission_towerfall
   ```

The override is intentionally not persisted. Repeat the UI selection after each process restart.

## Binary choices

### Safe navigation/reference build

Archive name:

```text
artifacts\steam_api64.homecoming-safe-no-active-scene.dll
```

SHA-256:

```text
28D7730FCA18F1856FB97D091AB3D55ECFD0BA2166343125788BF4694716E13E
```

This predates active Scene-entry publication. It is the build to use for entering Homecoming and validating the objective/dialogue path without triggering the known Scene crash.

### Current diagnostic build

Archive name:

```text
artifacts\steam_api64.scene-reference-crash-repro.dll
```

SHA-256:

```text
802F55DD1091BA43D227C038D70EEBBD64A1BD5A28E205F79781D401C1DA19D1
```

This contains the corrected `5 + 5` Scene array-count split and publishes the recovered active entry. It is expected to crash at the wall-breach handoff and should only be used to reproduce or instrument the fault.

## Latest Scene result

The active Scene state is produced by:

```text
registry/slot = 0x9D8076E4 / type 43 / index 5
schema        = 0x8080626B
selector      = 0x80B82771
entry         = 0x57318E3B / type 2 / index 1
body width    = 129 bits
```

The schema shape is now proven:

```text
74 base bits + 55 bits per active entry + 32 bits per word
```

For one entry and zero words this is `74 + 55 = 129` bits. Both bounded-array counts are five bits. The prior `4 + 6` split decoded one entry as two and shifted the registry left; that defect is fixed.

The corrected client runtime snapshot is:

```text
r0  = 80B82771
r4  = 00000000
r8  = 00000001
rC  = 57318E3B
r10 = 00010002
r4C = 02000000
```

The corresponding log line is near line 2320 of the packaged `evidence\logs\sunrise.scene-count-correct.log`.

## Crash evidence

Both active-entry runs fail at the same native address and player position. The corrected run reports:

```text
exception: EXCEPTION_ACCESS_VIOLATION
fault VA:  0x00007FF654E9309B
image:     0x00007FF654400000
fault RVA: 0x00A9309B
read:      0x0000000000000314
game tick: 394
position:  21.224665, 117.822441, -10.273363
```

Exception-time registers and instruction:

```text
RAX = 0
RCX = 0x314
RDX = 0x2F0
RBX = stack local containing 0x314

destiny2.exe+0xA9309B: cmp eax, dword ptr [rcx]
```

The immediate return address on the captured stack is `destiny2.exe+0xA92B07`. The corrected decoded Scene runtime is present and valid in the same dump, so the failure occurs after authority application while Destiny is resolving or reconciling the Scene's runtime entry.

Packaged evidence:

```text
evidence\logs\sunrise.scene-count-correct.log
evidence\logs\sunrise.scene-count-shifted.log
evidence\crash-current\crash_info.txt
evidence\crash-current\global_status_info.txt
evidence\crash-current\minidump.dmp
evidence\crash-previous\crash_info.txt
```

## Most likely interpretation

The `0x80809C25` table inside selector definition `0x80B82771` contains several package references. The invariant `0x57318E3B/2/1` row was recovered correctly from offset `0x128` of:

```text
Sunrise\analysis\towerfall_cue_ref_80B82771_class_80809C0F.bin
```

However, that row is not one of the activity roster's registered group identities. The Scene consumer reaches native processing but constructs a local record whose expected backing pointer is the small value `0x314`. The strongest current conclusion is that the row is a child/action anchor and needs a package-owned Scene runtime object to be created or selected first.

## Immediate next work

1. Disable active Towerfall Scene publication for ordinary runs, or use the safe DLL.
2. Instrument the native caller at `+0xA92B07` and the faulting consumer at `+0xA9309B`.
3. Capture the input/output object and stack-local vector immediately before `+0xA9309B` during:
   - Omega's successful Scene start.
   - Towerfall's failed wall-breach start.
4. Correlate that object with the existing Scene authority/apply surfaces:
   - authority decode: `+0x4C72E0`
   - Scene apply: `+0xB41DD0`
   - component start: `+0xB31910`
5. Determine which selector/reference creates a real backing pointer instead of `0x314`.
6. Add a fail-closed validity gate so a small/unresolved context cannot reach the native consumer.
7. Only after the Scene resolves, activate the associated type-1 Cabal spawner entries and validate encounter-clear/path-unlock progression.

Do not paper over `+0xA9309B` by skipping the faulting instruction. That would hide the missing runtime binding and leave the Scene partially initialized.

## Important source locations

```text
Sunrise\src\middleware\bap\activity_message\activity_sensor_auth_bodies.cpp
Sunrise\src\middleware\bap\activity_message\activity_sensor_auth_encoder.cpp
Sunrise\src\middleware\bap\activity_message\sensor_auth_update.h
Sunrise\src\middleware\bap\activity_message\tower_watch_cue_manifest.h
Sunrise\src\server\bap\encrypted\push\activity\activity_roster_snapshot.cpp
Sunrise\src\client\hooks\bootflow\activity_script_event_probe.cpp
Sunrise\src\client\hooks\bootflow\activity_spawner_chain_probe.cpp
Sunrise\src\client\hooks\bootflow\opening_authority\scene_authority_capture.*
Sunrise\src\state\activity\forced\activity_forced_destination.cpp
Sunrise\src\state\activity\forced\definition.h
Sunrise\src\server\ui\activity_override\activity_override_panel.cpp
```

## Building

From `C:\Destiny 2 Development` in PowerShell:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Sunrise\Sunrise.vcxproj /m /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
```

Output:

```text
C:\Destiny 2 Development\build\x64\Release\steam_api64.dll
```

Always close Destiny before copying the DLL to the root and `bin` locations.

## Package scope

The handoff ZIP intentionally excludes the installed game depot, `packages`, root `bin`, generated build trees, and caches. Those are large and reproducible. It includes the source snapshot, project metadata, settings, extracted Towerfall evidence, selected reference logs, crash evidence, and the two labeled DLL artifacts.

There is no Git repository metadata in the current workspace, so this ZIP is the authoritative source snapshot for the handoff.
